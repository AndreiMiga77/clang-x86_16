//===-- I8086FixupByteMask.cpp - Byte-mask AND -> XOR peephole ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A masking `and reg, 0x00FF` (zero-extend a byte) or `and reg, 0xFF00` is, when
// the register lives in AX/BX/CX/DX, just clearing one 8-bit half of the word.
// The 8086 can do that with a two-byte `xor <half>,<half>` instead of a 3-or-4
// byte immediate AND.  Whether the value is in a byte-addressable register is
// only known after register allocation, and this rewrite drops the AND's flag
// result, so it runs post-RA and only when those flags are dead.  Registers
// without a byte subregister (SI/DI/BP) keep the plain AND, which is why the
// isel pattern for `and reg,0xFF` is left in place.
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
    return "I8086 byte-mask AND->XOR peephole";
  }
};
} // namespace

char I8086FixupByteMask::ID = 0;

bool I8086FixupByteMask::runOnMachineFunction(MachineFunction &MF) {
  const I8086InstrInfo &TII = *MF.getSubtarget<I8086Subtarget>().getInstrInfo();
  const TargetRegisterInfo &TRI =
      *MF.getSubtarget().getRegisterInfo();
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
