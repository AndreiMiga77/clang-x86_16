//===-- I8086RegisterInfo.cpp - I8086 Register Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8086RegisterInfo.h"
#include "I8086.h"
#include "I8086Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "I8086GenRegisterInfo.inc"

I8086RegisterInfo::I8086RegisterInfo() : I8086GenRegisterInfo(I8086::IP) {}

const MCPhysReg *
I8086RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  // BP is always reserved (frame pointer when a frame exists, otherwise unused)
  // and is saved/restored by the prologue/epilogue, so it is never in the CSR
  // list handled by the generic spill machinery.
  static const MCPhysReg CSR[] = {I8086::SI, I8086::DI, 0};
  return CSR;
}

BitVector I8086RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  // SP and BP are never allocatable: SP cannot be a ModR/M base, and BP is
  // reserved as the frame pointer.
  Reserved.set(I8086::SP);
  Reserved.set(I8086::BP);
  Reserved.set(I8086::IP);
  Reserved.set(I8086::FLAGS);
  return Reserved;
}

const TargetRegisterClass *
I8086RegisterInfo::getPointerRegClass(unsigned Kind) const {
  return &I8086::GR16RegClass;
}

bool I8086RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected SP adjustment");

  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const TargetFrameLowering *TFI = getFrameLowering(MF);
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  Register BasePtr = TFI->hasFP(MF) ? I8086::BP : I8086::SP;
  int Offset = MF.getFrameInfo().getObjectOffset(FrameIndex);

  // Account for the saved return address (near call = 2 bytes).
  Offset += 2;
  if (!TFI->hasFP(MF))
    Offset += MF.getFrameInfo().getStackSize();
  else
    Offset += 2; // saved BP

  if (MI.getOpcode() == I8086::ADDframe) {
    // Address of a stack slot.  Operands are (dst, base(FI), offset).  The
    // 8086 has only two-address adds, so materialize it as
    //   mov dst, base ; [add dst, offset]
    Offset += MI.getOperand(FIOperandNum + 1).getImm();
    MI.setDesc(TII.get(I8086::MOV16rr));
    MI.getOperand(FIOperandNum).ChangeToRegister(BasePtr, false);
    MI.removeOperand(FIOperandNum + 1);

    if (Offset == 0)
      return false;

    Register DstReg = MI.getOperand(0).getReg();
    DebugLoc DL2 = MI.getDebugLoc();
    if (Offset < 0)
      BuildMI(MBB, std::next(II), DL2, TII.get(I8086::SUB16ri), DstReg)
          .addReg(DstReg).addImm(-Offset);
    else
      BuildMI(MBB, std::next(II), DL2, TII.get(I8086::ADD16ri), DstReg)
          .addReg(DstReg).addImm(Offset);
    return false;
  }

  // Memory operand: (base, index, disp, seg).  FIOperandNum is the base; the
  // displacement is two operands later.
  Offset += MI.getOperand(FIOperandNum + 2).getImm();
  MI.getOperand(FIOperandNum).ChangeToRegister(BasePtr, false);
  MI.getOperand(FIOperandNum + 2).ChangeToImmediate(Offset);
  return false;
}

Register I8086RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  const TargetFrameLowering *TFI = getFrameLowering(MF);
  return TFI->hasFP(MF) ? I8086::BP : I8086::SP;
}
