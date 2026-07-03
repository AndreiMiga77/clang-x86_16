//===-- I8086CompactEncoding.cpp - Accumulator short-form peephole --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Post-RA size peephole: an ALU r/m,imm whose destination ended up in AL/AX can
// use the one-byte-shorter accumulator opcode (0x04..0x3D).  Only the imm16
// (0x81) and 8-bit forms are rewritten; the 0x83 imm8 form is already 3 bytes,
// the same as the accumulator form.  The accumulator form is semantically
// identical (same result and flags), so this is a plain in-place opcode swap
// with no liveness check.
//
// Runs in addPreEmitPass before branch relaxation (so sizes are final) and
// after the byte-mask pass (so `and ax,0xFF` has already become `xor ah,ah`).
//
//===----------------------------------------------------------------------===//

#include "I8086.h"
#include "I8086InstrInfo.h"
#include "I8086Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

using namespace llvm;

#define DEBUG_TYPE "i8086-compact-encoding"

namespace {
class I8086CompactEncoding : public MachineFunctionPass {
public:
  static char ID;
  I8086CompactEncoding() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }

  StringRef getPassName() const override {
    return "I8086 accumulator short-form peephole";
  }
};
} // namespace

char I8086CompactEncoding::ID = 0;

// r/m,imm ALU opcode -> its AL/AX accumulator short form.  Only the forms that
// actually shrink are listed (imm16 0x81 group and all 8-bit ops); the 0x83
// imm8 forms are already minimal.
static unsigned accumulatorForm(unsigned Opc) {
  switch (Opc) {
  case I8086::ADD8ri:  return I8086::ADD8i;
  case I8086::ADD16ri: return I8086::ADD16i;
  case I8086::OR8ri:   return I8086::OR8i;
  case I8086::OR16ri:  return I8086::OR16i;
  case I8086::ADC8ri:  return I8086::ADC8i;
  case I8086::ADC16ri: return I8086::ADC16i;
  case I8086::SBB8ri:  return I8086::SBB8i;
  case I8086::SBB16ri: return I8086::SBB16i;
  case I8086::AND8ri:  return I8086::AND8i;
  case I8086::AND16ri: return I8086::AND16i;
  case I8086::SUB8ri:  return I8086::SUB8i;
  case I8086::SUB16ri: return I8086::SUB16i;
  case I8086::XOR8ri:  return I8086::XOR8i;
  case I8086::XOR16ri: return I8086::XOR16i;
  case I8086::CMP8ri:  return I8086::CMP8i;
  case I8086::CMP16ri: return I8086::CMP16i;
  default:             return 0;
  }
}

bool I8086CompactEncoding::runOnMachineFunction(MachineFunction &MF) {
  const I8086InstrInfo &TII = *MF.getSubtarget<I8086Subtarget>().getInstrInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      unsigned Acc = accumulatorForm(MI.getOpcode());
      if (!Acc)
        continue;
      // Operand 0 is the destination (ALU) or the compared register (CMP); in
      // both the r/m,imm and accumulator forms it is that same register, so an
      // in-place opcode swap is enough when it is AL/AX.
      Register R = MI.getOperand(0).getReg();
      if (R != I8086::AL && R != I8086::AX)
        continue;
      MI.setDesc(TII.get(Acc));
      Changed = true;
    }
  }

  return Changed;
}

FunctionPass *llvm::createI8086CompactEncodingPass() {
  return new I8086CompactEncoding();
}
