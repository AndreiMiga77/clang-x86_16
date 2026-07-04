//===-- I8086OptCmpZero.cpp - Optimize equality tests against zero -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Post-RA peephole for `cmp reg, 0` when the result feeds only equality
// branches (je/jne, i.e. tests of ZF):
//
//   * If the instruction that produced `reg` is a flag-setting ALU op (add,
//     sub, inc, dec, and, or, xor, neg), it already set ZF = (reg == 0), so the
//     compare is redundant and is deleted:
//         dec si ; cmp si,0 ; jne   ->   dec si ; jne
//         sub cx,2 ; cmp cx,0 ; je  ->   sub cx,2 ; je
//
//   * Otherwise the compare is rewritten to `test reg, reg`, which is one byte
//     shorter than `cmp reg, 0`.  `test` (reg AND reg) sets ZF from the value,
//     so it is a valid substitute for an equality test; CF/OF are merely
//     cleared and are not meaningful, which is why this is restricted to je/jne.
//
// A memory-operand compare (`cmp [mem], 0`) is left alone: there is no cheaper
// equivalent.  Runs before branch relaxation so the size change is final.
//
//===----------------------------------------------------------------------===//

#include "I8086.h"
#include "I8086InstrInfo.h"
#include "I8086Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "i8086-opt-cmp-zero"

namespace {
class I8086OptCmpZero : public MachineFunctionPass {
  const I8086InstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;

public:
  static char ID;
  I8086OptCmpZero() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }

  StringRef getPassName() const override { return "I8086 optimize cmp-zero"; }

private:
  bool tryOpt(MachineBasicBlock &MBB, MachineInstr &Cmp);
};
} // namespace

char I8086OptCmpZero::ID = 0;

// `cmp reg, imm` forms with a register destination (not memory).
static bool isRegCmpImm(unsigned Opc) {
  return Opc == I8086::CMP8ri || Opc == I8086::CMP16ri ||
         Opc == I8086::CMP16ri8;
}

// ALU ops that write their register destination and set ZF = (result == 0).
// Deliberately excludes rotates (leave ZF untouched), NOT (kept flag-neutral
// here), MOV/LEA (no flags), and MUL/DIV (ZF undefined).
static bool setsZeroFlagFromResult(unsigned Opc) {
  switch (Opc) {
  case I8086::INC8r:  case I8086::INC16r:
  case I8086::DEC8r:  case I8086::DEC16r:
  case I8086::NEG8r:  case I8086::NEG16r:
  case I8086::ADD8rr: case I8086::ADD16rr:
  case I8086::ADD8ri: case I8086::ADD16ri: case I8086::ADD16ri8:
  case I8086::ADD8rm: case I8086::ADD16rm:
  case I8086::ADD8i:  case I8086::ADD16i:
  case I8086::SUB8rr: case I8086::SUB16rr:
  case I8086::SUB8ri: case I8086::SUB16ri: case I8086::SUB16ri8:
  case I8086::SUB8rm: case I8086::SUB16rm:
  case I8086::SUB8i:  case I8086::SUB16i:
  case I8086::AND8rr: case I8086::AND16rr:
  case I8086::AND8ri: case I8086::AND16ri: case I8086::AND16ri8:
  case I8086::AND8rm: case I8086::AND16rm:
  case I8086::AND8i:  case I8086::AND16i:
  case I8086::OR8rr:  case I8086::OR16rr:
  case I8086::OR8ri:  case I8086::OR16ri:  case I8086::OR16ri8:
  case I8086::OR8rm:  case I8086::OR16rm:
  case I8086::OR8i:   case I8086::OR16i:
  case I8086::XOR8rr: case I8086::XOR16rr:
  case I8086::XOR8ri: case I8086::XOR16ri: case I8086::XOR16ri8:
  case I8086::XOR8rm: case I8086::XOR16rm:
  case I8086::XOR8i:  case I8086::XOR16i:
    return true;
  default:
    return false;
  }
}

bool I8086OptCmpZero::tryOpt(MachineBasicBlock &MBB, MachineInstr &Cmp) {
  if (!isRegCmpImm(Cmp.getOpcode()))
    return false;
  if (!Cmp.getOperand(0).isReg() || !Cmp.getOperand(1).isImm() ||
      Cmp.getOperand(1).getImm() != 0)
    return false;
  Register Reg = Cmp.getOperand(0).getReg();

  // Every consumer of the FLAGS this compare produces must be a je/jne (an
  // equality test of ZF).  Scan forward until FLAGS is redefined; require at
  // least one reader and no reader that is not je/jne (so FLAGS is not left
  // live-out to an unknown user either).
  bool SawEqUser = false;
  for (auto J = std::next(Cmp.getIterator()); J != MBB.end(); ++J) {
    if (J->isDebugInstr())
      continue;
    if (J->readsRegister(I8086::FLAGS, TRI)) {
      if (J->getOpcode() != I8086::JE && J->getOpcode() != I8086::JNE)
        return false;
      SawEqUser = true;
    }
    if (J->definesRegister(I8086::FLAGS, TRI))
      break; // FLAGS redefined; later readers don't see this compare.
  }
  if (!SawEqUser)
    return false;

  // #1: if the immediately preceding real instruction produced `reg` with a
  // flag-setting ALU op, its ZF already answers the equality, so drop the cmp.
  auto P = Cmp.getReverseIterator();
  for (++P; P != MBB.rend() && P->isDebugInstr(); ++P)
    ;
  if (P != MBB.rend() && setsZeroFlagFromResult(P->getOpcode()) &&
      P->getOperand(0).isReg() && P->getOperand(0).getReg() == Reg &&
      P->definesRegister(I8086::FLAGS, TRI)) {
    Cmp.eraseFromParent();
    return true;
  }

  // #2: otherwise shrink `cmp reg,0` to `test reg,reg`.
  unsigned TestOpc =
      Cmp.getOpcode() == I8086::CMP8ri ? I8086::TEST8rr : I8086::TEST16rr;
  BuildMI(MBB, Cmp, Cmp.getDebugLoc(), TII->get(TestOpc))
      .addReg(Reg)
      .addReg(Reg);
  Cmp.eraseFromParent();
  return true;
}

bool I8086OptCmpZero::runOnMachineFunction(MachineFunction &MF) {
  TII = MF.getSubtarget<I8086Subtarget>().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF)
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &MI = *I++;
      if (tryOpt(MBB, MI))
        Changed = true;
    }
  return Changed;
}

FunctionPass *llvm::createI8086OptCmpZeroPass() { return new I8086OptCmpZero(); }
