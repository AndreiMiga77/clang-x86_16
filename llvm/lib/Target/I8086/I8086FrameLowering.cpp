//===-- I8086FrameLowering.cpp - Frame lowering for I8086 ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Prologue/epilogue for the 8086.  There is no LEAVE/ENTER (186+), so frames
// are built with explicit push/mov/pop.  Note: SP cannot be a ModR/M base
// register on the 8086, so any function that accesses its frame must use BP as
// a frame pointer -- hasFPImpl forces this whenever there are stack objects.
// DOS .COM images carry no CFI/unwind info, so none is emitted.
//
//===----------------------------------------------------------------------===//

#include "I8086FrameLowering.h"
#include "I8086InstrInfo.h"
#include "I8086MachineFunctionInfo.h"
#include "I8086Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

I8086FrameLowering::I8086FrameLowering(const I8086Subtarget &STI)
    : TargetFrameLowering(TargetFrameLowering::StackGrowsDown, Align(2), -2,
                          Align(2)),
      STI(STI), TII(*STI.getInstrInfo()), TRI(STI.getRegisterInfo()) {}

bool I8086FrameLowering::hasFPImpl(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  // The 8086 can only address memory through BX/BP/SI/DI, never SP, so any
  // function with a frame must keep BP as a frame pointer.
  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
         MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken() ||
         MFI.hasStackObjects();
}

bool I8086FrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  // Outgoing arguments are passed by PUSH, which changes SP per call, so the
  // call frame is never reserved; the caller cleans up with `add sp` (cdecl).
  return false;
}

void I8086FrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  assert(&MF.front() == &MBB && "Shrink-wrapping not yet supported");
  MachineFrameInfo &MFI = MF.getFrameInfo();
  I8086MachineFunctionInfo *FI = MF.getInfo<I8086MachineFunctionInfo>();

  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  uint64_t StackSize = MFI.getStackSize();
  uint64_t NumBytes = 0;

  if (hasFP(MF)) {
    // push bp ; mov bp, sp
    uint64_t FrameSize = StackSize - 2;
    NumBytes = FrameSize - FI->getCalleeSavedFrameSize();

    BuildMI(MBB, MBBI, DL, TII.get(I8086::PUSH16r))
        .addReg(I8086::BP, RegState::Kill)
        .setMIFlag(MachineInstr::FrameSetup);
    BuildMI(MBB, MBBI, DL, TII.get(I8086::MOV16rr), I8086::BP)
        .addReg(I8086::SP)
        .setMIFlag(MachineInstr::FrameSetup);

    for (MachineBasicBlock &MBBJ : llvm::drop_begin(MF))
      MBBJ.addLiveIn(I8086::BP);
  } else {
    NumBytes = StackSize - FI->getCalleeSavedFrameSize();
  }

  // Skip the callee-saved register pushes inserted by
  // spillCalleeSavedRegisters.
  while (MBBI != MBB.end() && MBBI->getFlag(MachineInstr::FrameSetup) &&
         MBBI->getOpcode() == I8086::PUSH16r)
    ++MBBI;

  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();

  // sub sp, NumBytes  (reserve local area).
  if (NumBytes) {
    MachineInstr *MI = BuildMI(MBB, MBBI, DL, TII.get(I8086::SUB16ri), I8086::SP)
                           .addReg(I8086::SP)
                           .addImm(NumBytes)
                           .setMIFlag(MachineInstr::FrameSetup);
    MI->getOperand(3).setIsDead(); // FLAGS def is dead.
  }
}

void I8086FrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  I8086MachineFunctionInfo *FI = MF.getInfo<I8086MachineFunctionInfo>();

  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  DebugLoc DL = MBBI->getDebugLoc();

  switch (MBBI->getOpcode()) {
  case I8086::RET:
  case I8086::RETI:
  case I8086::RETF:
  case I8086::RETFI:
    break;
  default:
    llvm_unreachable("Can only insert epilogue into returning blocks");
  }

  uint64_t StackSize = MFI.getStackSize();
  unsigned CSSize = FI->getCalleeSavedFrameSize();
  uint64_t NumBytes = 0;

  if (hasFP(MF)) {
    uint64_t FrameSize = StackSize - 2;
    NumBytes = FrameSize - CSSize;
  } else {
    NumBytes = StackSize - CSSize;
  }

  // The return terminator; the saved BP must be popped right before it, after
  // any callee-saved (SI/DI) pops that restoreCalleeSavedRegisters inserted.
  MachineBasicBlock::iterator RetI = MBBI;

  // Find the first callee-saved pop (they precede the return).
  MachineBasicBlock::iterator FirstCSPop = RetI;
  while (FirstCSPop != MBB.begin()) {
    MachineBasicBlock::iterator PI = std::prev(FirstCSPop);
    if (PI->getOpcode() == I8086::POP16r &&
        PI->getFlag(MachineInstr::FrameDestroy))
      FirstCSPop = PI;
    else
      break;
  }
  DL = FirstCSPop->getDebugLoc();

  // Restore SP just before the callee-saved pops.
  if (MFI.hasVarSizedObjects()) {
    // mov sp, bp ; sub sp, CSSize  -> point SP at the callee-saved area.
    BuildMI(MBB, FirstCSPop, DL, TII.get(I8086::MOV16rr), I8086::SP)
        .addReg(I8086::BP)
        .setMIFlag(MachineInstr::FrameDestroy);
    if (CSSize) {
      MachineInstr *MI =
          BuildMI(MBB, FirstCSPop, DL, TII.get(I8086::SUB16ri), I8086::SP)
              .addReg(I8086::SP)
              .addImm(CSSize)
              .setMIFlag(MachineInstr::FrameDestroy);
      MI->getOperand(3).setIsDead();
    }
  } else if (NumBytes) {
    // add sp, NumBytes  (free the local area).
    MachineInstr *MI =
        BuildMI(MBB, FirstCSPop, DL, TII.get(I8086::ADD16ri), I8086::SP)
            .addReg(I8086::SP)
            .addImm(NumBytes)
            .setMIFlag(MachineInstr::FrameDestroy);
    MI->getOperand(3).setIsDead();
  }

  // pop bp (after the SI/DI pops, immediately before the return).
  if (hasFP(MF))
    BuildMI(MBB, RetI, RetI->getDebugLoc(), TII.get(I8086::POP16r), I8086::BP)
        .setMIFlag(MachineInstr::FrameDestroy);
}

bool I8086FrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return false;

  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  MachineFunction &MF = *MBB.getParent();
  I8086MachineFunctionInfo *FI = MF.getInfo<I8086MachineFunctionInfo>();
  // BP is saved by emitPrologue, not counted here.
  unsigned Count = 0;
  for (const CalleeSavedInfo &I : CSI) {
    if (I.getReg() == I8086::BP)
      continue;
    MBB.addLiveIn(I.getReg());
    BuildMI(MBB, MI, DL, TII.get(I8086::PUSH16r))
        .addReg(I.getReg(), RegState::Kill)
        .setMIFlag(MachineInstr::FrameSetup);
    ++Count;
  }
  FI->setCalleeSavedFrameSize(Count * 2);
  return true;
}

bool I8086FrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return false;

  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  for (const CalleeSavedInfo &I : llvm::reverse(CSI)) {
    if (I.getReg() == I8086::BP)
      continue; // BP restored by emitEpilogue.
    BuildMI(MBB, MI, DL, TII.get(I8086::POP16r), I.getReg())
        .setMIFlag(MachineInstr::FrameDestroy);
  }
  return true;
}

MachineBasicBlock::iterator I8086FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  MachineInstr &Old = *I;
  // ADJCALLSTACKDOWN: nothing to do -- the outgoing arguments are PUSHed, which
  // already reserves the stack space.
  // ADJCALLSTACKUP: cdecl caller cleanup -- `add sp, <arg bytes>`.
  if (Old.getOpcode() == TII.getCallFrameDestroyOpcode()) {
    uint64_t Amount = TII.getFrameSize(Old);
    Amount -= TII.getFramePoppedByCallee(Old); // cdecl: callee pops nothing.
    if (Amount) {
      Amount = alignTo(Amount, getStackAlign());
      MachineInstr *New =
          BuildMI(MF, Old.getDebugLoc(), TII.get(I8086::ADD16ri), I8086::SP)
              .addReg(I8086::SP)
              .addImm(Amount);
      New->getOperand(3).setIsDead(); // FLAGS def is dead.
      MBB.insert(I, New);
    }
  }
  return MBB.erase(I);
}

void I8086FrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *) const {
  // Reserve a frame slot for the saved BP so the prologue/epilogue frame-size
  // accounting (StackSize - 2) lines up.  It must be the last (lowest-index)
  // object so eliminateFrameIndex's fixed +2 offsets are correct.
  if (hasFP(MF)) {
    int FrameIdx = MF.getFrameInfo().CreateFixedObject(2, -4, true);
    (void)FrameIdx;
    assert(FrameIdx == MF.getFrameInfo().getObjectIndexBegin() &&
           "Slot for BP must be last to be found!");
  }
}
