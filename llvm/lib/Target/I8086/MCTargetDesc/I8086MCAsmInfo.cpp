//===-- I8086MCAsmInfo.cpp - I8086 asm properties -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8086MCAsmInfo.h"
#include "llvm/TargetParser/Triple.h"
using namespace llvm;

void I8086MCAsmInfo::anchor() {}

I8086MCAsmInfo::I8086MCAsmInfo(const Triple &TT) {
  CodePointerSize = 2;
  CalleeSaveStackSlotSize = 2;
  MaxInstLength = 6;

  CommentString = ";";

  // 8086/DOS assembly conventionally uses byte-granular alignment.
  AlignmentIsInBytes = true;
  UsesELFSectionDirectiveForBSS = true;

  SupportsDebugInformation = false;
  ExceptionsType = ExceptionHandling::None;
}
