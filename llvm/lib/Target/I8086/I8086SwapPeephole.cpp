//===-- I8086SwapPeephole.cpp - Fold a 3-MOV register swap into XCHG ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// LLVM has no register-swap primitive: a swap of two registers is lowered by the
// register allocator into three MOVs through a scratch register
//
//     mov T, X            ; save X
//     ... (no interference)
//     mov X, Y
//     ... (no interference)
//     mov Y, T            ; = old X
//
// On the 8086 this is 6 cycles and burns a scratch register (often a callee-saved
// SI/DI needing push/pop).  A single XCHG does it in 4 cycles (3 when AX is
// involved) with no scratch.  This post-RA peephole recognizes the idiom -- even
// when the three moves are not adjacent -- and rewrites the middle move into
// `xchg X, Y`, dropping the other two moves when the scratch dies.  It is modeled
// on AMDGPU's SIShrinkInstructions::matchSwap.
//
// It only ever produces a register/register XCHG (never `xchg mem`, whose
// implicit LOCK is catastrophic on the 386+), because it only matches reg->reg
// MOVs.  Runs before branch relaxation so the size change is accounted for.
//
//===----------------------------------------------------------------------===//

#include "I8086.h"
#include "I8086InstrInfo.h"
#include "I8086Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "i8086-swap-peephole"

namespace {
class I8086SwapPeephole : public MachineFunctionPass {
  const I8086InstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;

public:
  static char ID;
  I8086SwapPeephole() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }

  StringRef getPassName() const override { return "I8086 swap-to-XCHG peephole"; }

private:
  // On a match, rewrites the swap and advances I past it; returns true.
  bool matchSwap(MachineBasicBlock::iterator &I) const;
};
} // namespace

char I8086SwapPeephole::ID = 0;

bool I8086SwapPeephole::matchSwap(MachineBasicBlock::iterator &I) const {
  MachineInstr &MovT = *I;
  unsigned MovOpc = MovT.getOpcode();
  bool Is16 = MovOpc == I8086::MOV16rr;
  if (!Is16 && MovOpc != I8086::MOV8rr)
    return false;

  Register T = MovT.getOperand(0).getReg(); // scratch
  Register X = MovT.getOperand(1).getReg(); // saved value
  if (T == X)
    return false;

  MachineBasicBlock &MBB = *MovT.getParent();
  const unsigned SearchLimit = 16;
  unsigned Count = 0;
  bool KilledT = false;

  for (auto Iter = std::next(MovT.getIterator()), E = MBB.instr_end();
       Iter != E && Count < SearchLimit && !KilledT; ++Iter) {
    MachineInstr *MovY = &*Iter;
    KilledT = MovY->killsRegister(T, TRI);
    if (MovY->isDebugInstr())
      continue;
    ++Count;

    // Looking for  mov Y, T.
    if (MovY->getOpcode() != MovOpc || MovY->getOperand(1).getReg() != T)
      continue;
    Register Y = MovY->getOperand(0).getReg();
    if (Y == T || Y == X)
      continue;

    // Find  mov X, Y  in between, verifying nothing else disturbs X/Y/T.
    MachineInstr *MovX = nullptr;
    bool ReadsT = false;
    for (auto J = std::next(MovT.getIterator()); J != MovY->getIterator(); ++J) {
      if (J->isDebugInstr())
        continue;
      if (J->readsRegister(T, TRI))
        ReadsT = true;
      if (J->readsRegister(X, TRI) || J->modifiesRegister(Y, TRI) ||
          J->modifiesRegister(T, TRI) ||
          (MovX && J->modifiesRegister(X, TRI))) {
        MovX = nullptr;
        break;
      }
      if (!J->readsRegister(Y, TRI)) {
        if (!MovX && J->modifiesRegister(X, TRI)) {
          MovX = nullptr;
          break;
        }
        continue;
      }
      // J reads Y: it must be exactly the middle  mov X, Y.
      if (MovX || J->getOpcode() != MovOpc || J->getOperand(0).getReg() != X) {
        MovX = nullptr;
        break;
      }
      MovX = &*J;
    }
    if (!MovX)
      continue;

    // Emit XCHG X, Y at the middle move, preferring the 1-byte AX form.  Both
    // registers are read and written, so add them as implicit defs.
    unsigned Opc;
    Register E0, E1;
    if (Is16 && (X == I8086::AX || Y == I8086::AX)) {
      Opc = I8086::XCHG16ar;
      E0 = X == I8086::AX ? Y : X; // the encoded (non-AX) register
      E1 = I8086::AX;
    } else {
      Opc = Is16 ? I8086::XCHG16rr : I8086::XCHG8rr;
      E0 = X;
      E1 = Y;
    }
    BuildMI(MBB, *MovX, MovX->getDebugLoc(), TII->get(Opc))
        .addReg(E0)
        .addReg(E1)
        .addReg(X, RegState::ImplicitDefine)
        .addReg(Y, RegState::ImplicitDefine);

    // Continue scanning after the (now removed) third move.
    I = std::next(MovY->getIterator());
    MovX->eraseFromParent();
    MovY->eraseFromParent();

    // The `mov T, X` is dead once its only consumer (MovY) is gone; otherwise
    // keep it but drop a now-stale kill of X (the XCHG still reads X).
    if (KilledT && !ReadsT)
      MovT.eraseFromParent();
    else if (MovT.getOperand(1).isKill())
      MovT.getOperand(1).setIsKill(false);

    return true;
  }

  return false;
}

bool I8086SwapPeephole::runOnMachineFunction(MachineFunction &MF) {
  TII = MF.getSubtarget<I8086Subtarget>().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF)
    for (auto I = MBB.begin(), E = MBB.end(); I != E;) {
      if (matchSwap(I))
        Changed = true;
      else
        ++I;
    }

  return Changed;
}

FunctionPass *llvm::createI8086SwapPeepholePass() {
  return new I8086SwapPeephole();
}
