//===-- I8086.cpp - I8086 target component anchor -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// I8086 is an assembler-only target and has no CodeGen library.  This file
// provides a small top-level "LLVMI8086" component library so that the
// "I8086" build component resolves for tools that link LLVM targets by name.
//
//===----------------------------------------------------------------------===//

namespace llvm {
void I8086TargetAnchor() {}
} // namespace llvm
