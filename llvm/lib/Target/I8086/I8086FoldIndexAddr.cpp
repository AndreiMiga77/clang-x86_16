//===-- I8086FoldIndexAddr.cpp - Fold add+access into [base+index] -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Post-RA peephole that folds an address-forming add into an indexed memory
// operand:
//
//     add   B, I          ; B in {BX,BP}, I already in {SI,DI}, B dead after
//     mov   reg, [B]      ; (or a store) single-register access, disp optional
//   =>
//     mov   reg, [B+I]    ; drop the add
//
// This is done *only* when the index I is already in a callee-saved index
// register (SI/DI) and the base in {BX,BP}, so the fold introduces no register
// move or spill -- it removes the add at the cost of the small indexed-EA
// penalty, which is a net win (e.g. ~5 clocks on the 8088, where the removed
// 2-byte add was fetch-bound).  Deciding this needs the physical register
// assignment, so it cannot be an isel pattern.
//
// A multiply-used address (the add result live past the access) is left alone:
// materializing it once and using [reg] at each use is cheaper than paying the
// indexed EA every time.  Runs before branch relaxation so sizes stay final.
//
//===----------------------------------------------------------------------===//

#include "I8086.h"
#include "I8086InstrInfo.h"
#include "I8086Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

using namespace llvm;

#define DEBUG_TYPE "i8086-fold-index-addr"

namespace {
class I8086FoldIndexAddr : public MachineFunctionPass {
  const I8086InstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;

public:
  static char ID;
  I8086FoldIndexAddr() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }

  StringRef getPassName() const override { return "I8086 fold index address"; }
};
} // namespace

char I8086FoldIndexAddr::ID = 0;

// The 8086 allows only base in {BX,BP} and index in {SI,DI}.
static bool isBaseReg(Register R) { return R == I8086::BX || R == I8086::BP; }
static bool isIndexReg(Register R) { return R == I8086::SI || R == I8086::DI; }

// The [base+index] load/store form of a single-register MOV load/store, and the
// operand index of that instruction's memory-operand base register (0 for the
// store forms whose memory operand is first, 1 for loads after the dst).
static unsigned indexedForm(unsigned Opc, unsigned &BaseOp) {
  switch (Opc) {
  case I8086::MOV8rm:  BaseOp = 1; return I8086::MOV8rm_idx;
  case I8086::MOV16rm: BaseOp = 1; return I8086::MOV16rm_idx;
  case I8086::MOV8mr:  BaseOp = 0; return I8086::MOV8mr_idx;
  case I8086::MOV16mr: BaseOp = 0; return I8086::MOV16mr_idx;
  default:             return 0;
  }
}

bool I8086FoldIndexAddr::runOnMachineFunction(MachineFunction &MF) {
  TII = MF.getSubtarget<I8086Subtarget>().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  bool Changed = false;

  const unsigned SearchLimit = 16;

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(), E = MBB.end(); I != E;) {
      MachineInstr &Add = *I++;
      if (Add.getOpcode() != I8086::ADD16rr)
        continue;

      Register B = Add.getOperand(0).getReg();   // dst == src1 (tied)
      Register Idx = Add.getOperand(2).getReg(); // src2
      if (!isBaseReg(B) || !isIndexReg(Idx))
        continue;
      // The add is dropped, so its FLAGS def must be dead (no consumer).
      if (!Add.registerDefIsDead(I8086::FLAGS, TRI))
        continue;

      // Scan forward for the single-register access of [B].  Bail if B or Idx is
      // disturbed first, or if B is read anywhere but that access.
      MachineInstr *Mem = nullptr;
      unsigned NewOpc = 0, BaseOp = 0;
      unsigned Count = 0;
      for (auto J = std::next(Add.getIterator());
           J != MBB.end() && Count < SearchLimit; ++J) {
        if (J->isDebugInstr())
          continue;
        ++Count;
        if (J->modifiesRegister(Idx, TRI))
          break; // index no longer holds the value

        unsigned Cand = indexedForm(J->getOpcode(), BaseOp);
        if (Cand && J->getOperand(BaseOp).getReg() == B &&
            !J->getOperand(BaseOp + 1).getReg() && // index slot empty
            J->getOperand(BaseOp + 2).isImm()) {   // plain displacement
          NewOpc = Cand;
          Mem = &*J;
          break;
        }
        if (J->readsRegister(B, TRI) || J->modifiesRegister(B, TRI))
          break; // B used/redefined other than by the access we want
      }
      if (!Mem)
        continue;

      // The add result must die at the access (single use): otherwise B is
      // needed as base+index later and dropping the add would corrupt it.
      if (!Mem->killsRegister(B, TRI))
        continue;

      // Rewrite the access to the indexed form, filling its index slot, and drop
      // the add.  B keeps its pre-add value, which is exactly the base.
      Mem->setDesc(TII->get(NewOpc));
      MachineOperand &IdxMO = Mem->getOperand(BaseOp + 1);
      IdxMO.setReg(Idx);
      IdxMO.setIsKill(Add.getOperand(2).isKill());
      Add.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

FunctionPass *llvm::createI8086FoldIndexAddrPass() {
  return new I8086FoldIndexAddr();
}
