//===-- I8086FoldMemImm.cpp - Fold a memory operand into reg,imm ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Post-RA peephole that turns
//     mov R, [mem]                (load)
//     <op> R, imm                 (R = R OP imm)
// into
//     mov R, imm
//     <op> R, [mem]               (R = imm OP mem)
// when it is strictly smaller.  isel keeps a constant operand as an immediate
// (the reg,imm forms outrank the load-fold); this decides -- with the actual
// register and immediate in hand -- whether pulling the memory operand in wins.
//
// It only fires when:
//   * the op is commutative (add/and/or/xor) -- for these `mem OP imm` equals
//     `imm OP mem` in both value AND flags, so the reorder is safe; sub/sbb are
//     order-sensitive and excluded;
//   * the reg,imm form is larger than `mov reg,imm` (any 8-bit op, or a 16-bit
//     full-imm16 0x81 op -- not the compact 0x83 form, which is neutral); and
//   * R is not AX/AL, which could load a [disp16] via the shorter moffs form,
//     making the non-folded sequence smaller there.
//
// Runs before branch relaxation so the size change is final.
//
//===----------------------------------------------------------------------===//

#include "I8086.h"
#include "I8086InstrInfo.h"
#include "I8086Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "i8086-fold-mem-imm"

namespace {
class I8086FoldMemImm : public MachineFunctionPass {
  const I8086InstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;

public:
  static char ID;
  I8086FoldMemImm() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }

  StringRef getPassName() const override {
    return "I8086 fold memory into reg,imm";
  }
};
} // namespace

char I8086FoldMemImm::ID = 0;

// The reg,mem form of a commutative reg,imm ALU op, returned only when folding
// shrinks: any 8-bit op (mov r,imm8 = 2B < op r,imm8 = 3B) and the 16-bit
// full-imm16 0x81 op (mov r,imm16 = 3B < op r,imm16 = 4B).  The compact 0x83
// (16ri8) form is neutral, and sub/sbb are order-sensitive; both return 0.
static unsigned memFormIfSmaller(unsigned RiOpc) {
  switch (RiOpc) {
  case I8086::ADD8ri:  return I8086::ADD8rm;
  case I8086::ADD16ri: return I8086::ADD16rm;
  case I8086::AND8ri:  return I8086::AND8rm;
  case I8086::AND16ri: return I8086::AND16rm;
  case I8086::OR8ri:   return I8086::OR8rm;
  case I8086::OR16ri:  return I8086::OR16rm;
  case I8086::XOR8ri:  return I8086::XOR8rm;
  case I8086::XOR16ri: return I8086::XOR16rm;
  default:             return 0;
  }
}

bool I8086FoldMemImm::runOnMachineFunction(MachineFunction &MF) {
  TII = MF.getSubtarget<I8086Subtarget>().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(), E = MBB.end(); I != E;) {
      MachineInstr &Mov = *I;
      unsigned MovOpc = Mov.getOpcode();
      bool Is16 = MovOpc == I8086::MOV16rm;
      if (!Is16 && MovOpc != I8086::MOV8rm) {
        ++I;
        continue;
      }
      Register R = Mov.getOperand(0).getReg();
      // AX/AL can load a [disp16] via the 1-byte-shorter moffs form.
      if (R == I8086::AX || R == I8086::AL) {
        ++I;
        continue;
      }

      // The next real instruction must be a foldable commutative reg,imm op on R
      // (two-address: dst == src1 == R, immediate in operand 2).  Adjacency
      // guarantees the load result feeds only this op and the address registers
      // are untouched.
      auto J = std::next(I);
      while (J != E && J->isDebugInstr())
        ++J;
      if (J == E) {
        ++I;
        continue;
      }
      MachineInstr &Op = *J;
      unsigned RmOpc = memFormIfSmaller(Op.getOpcode());
      if (!RmOpc || Op.getOperand(0).getReg() != R ||
          Op.getOperand(1).getReg() != R) {
        ++I;
        continue;
      }

      DebugLoc dl = Op.getDebugLoc();
      // mov R, imm  (the op's immediate).
      auto NewMov =
          BuildMI(MBB, Mov, Mov.getDebugLoc(),
                  TII->get(Is16 ? I8086::MOV16ri : I8086::MOV8ri), R);
      NewMov.add(Op.getOperand(2));
      // <op> R, [mem]  (two-address; mem = the load's base/index/disp/seg).
      auto NewOp = BuildMI(MBB, Op, dl, TII->get(RmOpc), R).addReg(R);
      for (unsigned K = 1, KE = Mov.getNumOperands(); K != KE; ++K)
        NewOp.add(Mov.getOperand(K));
      NewOp.cloneMemRefs(Mov);
      if (Op.registerDefIsDead(I8086::FLAGS, TRI))
        NewOp->addRegisterDead(I8086::FLAGS, TRI);

      auto Next = std::next(J);
      Op.eraseFromParent();
      Mov.eraseFromParent();
      I = Next;
      Changed = true;
    }
  }

  return Changed;
}

FunctionPass *llvm::createI8086FoldMemImmPass() { return new I8086FoldMemImm(); }
