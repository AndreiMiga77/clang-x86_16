//===-- I8086BaseInfo.h - Top level definitions for I8086 -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Small enums and helpers shared between the TableGen'd tables and the
// hand-written MC code emitter.  The numeric values here MUST match those in
// I8086InstrFormats.td.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_I8086_MCTARGETDESC_I8086BASEINFO_H
#define LLVM_LIB_TARGET_I8086_MCTARGETDESC_I8086BASEINFO_H

#include <cstdint>

namespace llvm {
namespace I8086II {

/// Encoding form.  Mirror of the Format defs in I8086InstrFormats.td.
enum Form {
  Pseudo = 0,
  RawFrm = 1,
  AddRegFrm = 2,
  MRMDestReg = 3,
  MRMDestMem = 4,
  MRMSrcReg = 5,
  MRMSrcMem = 6,
  RawFrmFar = 7,
  // Group /digit forms.
  MRM0r = 8,
  MRM7r = 15,
  MRM0m = 16,
  MRM7m = 23
};

/// Immediate kind.  Mirror of the ImmType defs in I8086InstrFormats.td.
enum ImmType {
  NoImm = 0,
  Imm8 = 1,
  Imm16 = 2,
  Rel8 = 3,
  Rel16 = 4
};

// TSFlags layout (see I8086InstrFormats.td):
//   bits 5-0   : Form
//   bits 8-6   : ImmType
//   bits 16-9  : opcode byte
enum {
  FormShift = 0,
  FormMask = 0x3f,
  ImmShift = 6,
  ImmMask = 0x7,
  OpcodeShift = 9,
  OpcodeMask = 0xff
};

inline unsigned getForm(uint64_t TSFlags) {
  return (TSFlags >> FormShift) & FormMask;
}
inline unsigned getImmType(uint64_t TSFlags) {
  return (TSFlags >> ImmShift) & ImmMask;
}
inline unsigned getOpcode(uint64_t TSFlags) {
  return (TSFlags >> OpcodeShift) & OpcodeMask;
}

/// Returns true for the group /digit forms (MRM0r..MRM7r, MRM0m..MRM7m).
inline bool isMRMGroup(unsigned Form) { return Form >= MRM0r && Form <= MRM7m; }

/// Returns the /digit extension for a group form.
inline unsigned getMRMExtension(unsigned Form) {
  return Form >= MRM0m ? Form - MRM0m : Form - MRM0r;
}

/// Returns true if the group form addresses a register operand (mod == 11).
inline bool isMRMGroupReg(unsigned Form) {
  return Form >= MRM0r && Form <= MRM7r;
}

} // namespace I8086II
} // namespace llvm

#endif // LLVM_LIB_TARGET_I8086_MCTARGETDESC_I8086BASEINFO_H
