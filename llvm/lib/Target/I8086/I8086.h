//===-- I8086.h - Top-level interface for I8086 -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Entry points for the LLVM Intel 8086 backend.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_I8086_I8086_H
#define LLVM_LIB_TARGET_I8086_I8086_H

#include "MCTargetDesc/I8086MCTargetDesc.h"
#include "llvm/Target/TargetMachine.h"

namespace I8086CC {
// 8086 condition codes (encoded in the low nibble of the Jcc opcode).
enum CondCode {
  COND_O = 0,   // overflow
  COND_NO = 1,  // no overflow
  COND_B = 2,   // below (unsigned <)      aka C
  COND_AE = 3,  // above or equal (unsigned >=) aka NC
  COND_E = 4,   // equal / zero
  COND_NE = 5,  // not equal / not zero
  COND_BE = 6,  // below or equal (unsigned <=)
  COND_A = 7,   // above (unsigned >)
  COND_S = 8,   // sign
  COND_NS = 9,  // no sign
  COND_P = 10,  // parity even
  COND_NP = 11, // parity odd
  COND_L = 12,  // less (signed <)
  COND_GE = 13, // greater or equal (signed >=)
  COND_LE = 14, // less or equal (signed <=)
  COND_G = 15,  // greater (signed >)

  COND_INVALID = -1
};
}

namespace llvm {
class FunctionPass;
class I8086TargetMachine;
class PassRegistry;

FunctionPass *createI8086ISelDag(I8086TargetMachine &TM,
                                 CodeGenOptLevel OptLevel);

void initializeI8086DAGToDAGISelLegacyPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_I8086_I8086_H
