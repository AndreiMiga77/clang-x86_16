//===-- I8086FormLea.cpp - Collapse a mov/add chain into one LEA ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Post-RA peephole that folds a chain computing `base + index + disp` --
//     mov DST, R ; add DST, ...        (or an in-place `add DST, ...` chain)
// -- into one `lea DST, [base+index+disp]`.  LEA has a longer execution phase
// but that hides under prefetch, and the encoding is shorter:
//     lea r,[reg+disp]    (3-4 B) < mov r,reg (2) + add r,disp (2-4)
//     lea r,[b+i]         (2 B)   < mov r,b (2)   + add r,i (2)
//     lea r,[b+i+disp]    (3-4 B) < mov + add + add
// and, for a 3-operand address, even when DST is one of the inputs:
//     lea DST,[DST+i+disp] (3-4 B) < add DST,i (2) + add DST,disp (2-4).
//
// Only fires when the register operands are valid addressing registers (a base
// in {BX,BP}, an index in {SI,DI}) -- the sole constraint -- and the LEA is
// strictly smaller.  A lone register or lone displacement is left alone (those
// are just movs that run slower).  LEA sets no flags, so the chain's final flags
// must be dead.  Runs before branch relaxation so the size change is final.
//
//===----------------------------------------------------------------------===//

#include "I8086.h"
#include "I8086InstrInfo.h"
#include "I8086Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "i8086-form-lea"

namespace {
class I8086FormLea : public MachineFunctionPass {
  const I8086InstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;

public:
  static char ID;
  I8086FormLea() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }

  StringRef getPassName() const override { return "I8086 form LEA"; }

private:
  bool tryFormLea(MachineBasicBlock &MBB, MachineBasicBlock::iterator &It);
};
} // namespace

char I8086FormLea::ID = 0;

static bool isBaseReg(Register R) { return R == I8086::BX || R == I8086::BP; }
static bool isIndexReg(Register R) { return R == I8086::SI || R == I8086::DI; }

bool I8086FormLea::tryFormLea(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator &It) {
  MachineInstr &First = *It;
  unsigned Opc = First.getOpcode();

  Register DST, Base, Index;
  int64_t Disp = 0;
  SmallVector<MachineInstr *, 4> Chain;

  // Assign a register addend to the base or index slot; fail if it is not an
  // addressing register or the slot is taken.
  auto addReg = [&](Register R) -> bool {
    if (isBaseReg(R)) { if (Base) return false; Base = R; return true; }
    if (isIndexReg(R)) { if (Index) return false; Index = R; return true; }
    return false;
  };

  if (Opc == I8086::MOV16rr) {
    DST = First.getOperand(0).getReg();
    Register R0 = First.getOperand(1).getReg();
    if (R0 == DST || !addReg(R0))
      return false;
    Chain.push_back(&First);
  } else if (Opc == I8086::ADD16rr || Opc == I8086::ADD16ri ||
             Opc == I8086::ADD16ri8) {
    // In-place: DST itself is read as the base/index.
    DST = First.getOperand(0).getReg();
    if (First.getOperand(1).getReg() != DST || !addReg(DST))
      return false;
    if (Opc == I8086::ADD16rr) {
      Register R = First.getOperand(2).getReg();
      if (R == DST || !addReg(R))
        return false;
    } else {
      if (!First.getOperand(2).isImm())
        return false;
      Disp += First.getOperand(2).getImm();
    }
    Chain.push_back(&First);
  } else {
    return false;
  }

  // Extend with consecutive `add DST, reg/imm`.
  auto J = std::next(First.getIterator());
  for (; J != MBB.end(); ++J) {
    if (J->isDebugInstr())
      continue;
    unsigned O = J->getOpcode();
    bool IsAddRR = O == I8086::ADD16rr;
    bool IsAddRI = O == I8086::ADD16ri || O == I8086::ADD16ri8;
    if (!IsAddRR && !IsAddRI)
      break;
    // Only now are operands 0/1 known to be the dst/src1 registers.
    if (J->getOperand(0).getReg() != DST || J->getOperand(1).getReg() != DST)
      break;
    if (IsAddRR) {
      Register R = J->getOperand(2).getReg();
      if (R == DST || !addReg(R))
        break;
    } else {
      if (!J->getOperand(2).isImm())
        break;
      Disp += J->getOperand(2).getImm();
    }
    Chain.push_back(&*J);
  }

  if (Chain.size() < 2)
    return false;
  // Non-degenerate: a lone register with no displacement is just a mov.
  unsigned NumRegs = (Base ? 1 : 0) + (Index ? 1 : 0);
  if (NumRegs == 0 || (NumRegs == 1 && Disp == 0))
    return false;
  // LEA sets no flags; the chain's final flags must be dead.
  if (!Chain.back()->registerDefIsDead(I8086::FLAGS, TRI))
    return false;

  // Build the candidate and keep it only if it is strictly smaller.
  Register NoReg;
  MachineInstr *Lea = BuildMI(MBB, First, Chain.back()->getDebugLoc(),
                              TII->get(I8086::LEA16r), DST)
                          .addReg(Base)
                          .addReg(Index)
                          .addImm(Disp)
                          .addReg(NoReg);
  unsigned LeaSize = TII->getInstSizeInBytes(*Lea);
  unsigned OldSize = 0;
  for (MachineInstr *MI : Chain)
    OldSize += TII->getInstSizeInBytes(*MI);
  if (LeaSize >= OldSize) {
    Lea->eraseFromParent();
    return false;
  }

  It = std::next(Chain.back()->getIterator());
  for (MachineInstr *MI : Chain)
    MI->eraseFromParent();
  return true;
}

bool I8086FormLea::runOnMachineFunction(MachineFunction &MF) {
  TII = MF.getSubtarget<I8086Subtarget>().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF)
    for (auto I = MBB.begin(); I != MBB.end();)
      if (tryFormLea(MBB, I))
        Changed = true;
      else
        ++I;
  return Changed;
}

FunctionPass *llvm::createI8086FormLeaPass() { return new I8086FormLea(); }
