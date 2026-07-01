//===-- I8086FixupKinds.h - I8086 Specific Fixup Entries --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_I8086_MCTARGETDESC_I8086FIXUPKINDS_H
#define LLVM_LIB_TARGET_I8086_MCTARGETDESC_I8086FIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace I8086 {

// This table must be kept in the same order as the Infos array in
// I8086AsmBackend.cpp.
enum Fixups {
  // 16-bit absolute (offset within the segment).
  fixup_16 = FirstTargetFixupKind,
  // 8-bit absolute.
  fixup_8,
  // 16-bit PC-relative (near call/jmp rel16).
  fixup_16_pcrel,
  // 8-bit PC-relative (short/conditional jumps rel8).
  fixup_8_pcrel,

  // Marker
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};

} // namespace I8086
} // namespace llvm

#endif // LLVM_LIB_TARGET_I8086_MCTARGETDESC_I8086FIXUPKINDS_H
