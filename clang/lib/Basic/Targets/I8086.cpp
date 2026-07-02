//===--- I8086.cpp - Implement I8086 target feature support ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements Intel 8086 TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "I8086.h"
#include "clang/Basic/MacroBuilder.h"

using namespace clang;
using namespace clang::targets;

const char *const I8086TargetInfo::GCCRegNames[] = {
    "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
    "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh",
    "es", "cs", "ss", "ds", "ip", "flags"};

ArrayRef<const char *> I8086TargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

void I8086TargetInfo::getTargetDefines(const LangOptions &Opts,
                                       MacroBuilder &Builder) const {
  Builder.defineMacro("__i8086__");
  Builder.defineMacro("__I8086__");
  Builder.defineMacro("__INTEL_8086__");
}
