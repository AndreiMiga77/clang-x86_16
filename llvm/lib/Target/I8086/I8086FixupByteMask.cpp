//===-- I8086FixupByteMask.cpp - Byte-granularity AND rewrites ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Post-RA pass that rewrites byte-granularity operations once physical
// registers are known.  Currently it turns a masking `and reg,0x00FF` /
// `and reg,0xFF00` into a byte-clearing `xor <hi>,<hi>` / `xor <lo>,<lo>` when
// the register has a byte half (AX/BX/CX/DX) and the AND's flags are dead (2
// bytes vs 3-4).  (This is where byte-level shift lowerings will live too.)
//
// Runs in addPreEmitPass, before branch relaxation, so the sizes it changes are
// final when branch offsets are computed.  It runs before the accumulator
// short-form pass so `and ax,0xFF` becomes the 2-byte `xor ah,ah` rather than
// the 3-byte accumulator AND.
//
//===----------------------------------------------------------------------===//

#include "I8086.h"
#include "I8086InstrInfo.h"
#include "I8086Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "i8086-fixup-byte-mask"

namespace {
class I8086FixupByteMask : public MachineFunctionPass {
public:
  static char ID;
  I8086FixupByteMask() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }

  StringRef getPassName() const override {
    return "I8086 byte-mask rewrites";
  }
};
} // namespace

char I8086FixupByteMask::ID = 0;

bool I8086FixupByteMask::runOnMachineFunction(MachineFunction &MF) {
  const I8086InstrInfo &TII = *MF.getSubtarget<I8086Subtarget>().getInstrInfo();
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
      if (MI.getOpcode() != I8086::AND16ri || !MI.getOperand(2).isImm())
        continue;

      // Rewriting to XOR discards the AND's flags, so they must be unused.
      if (!MI.registerDefIsDead(I8086::FLAGS, &TRI))
        continue;

      uint64_t Imm = MI.getOperand(2).getImm() & 0xFFFF;
      unsigned SubIdx;
      if (Imm == 0x00FF)
        SubIdx = I8086::sub_8bit_hi; // keep low byte, clear the high byte
      else if (Imm == 0xFF00)
        SubIdx = I8086::sub_8bit_lo; // keep high byte, clear the low byte
      else
        continue;

      Register Reg = MI.getOperand(0).getReg();
      if (!Reg.isPhysical())
        continue;
      Register Byte = TRI.getSubReg(Reg, SubIdx);
      if (!Byte)
        continue; // SI/DI/BP have no byte half: leave the AND in place.

      // `xor <half>,<half>` zeroes that half; the other half (holding the kept
      // byte) is left untouched.
      BuildMI(MBB, MI, MI.getDebugLoc(), TII.get(I8086::XOR8rr), Byte)
          .addReg(Byte)
          .addReg(Byte);
      MI.eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}

FunctionPass *llvm::createI8086FixupByteMaskPass() {
  return new I8086FixupByteMask();
}
