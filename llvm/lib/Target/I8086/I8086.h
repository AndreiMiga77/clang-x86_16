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

// Post-RA byte-mask rewrites (e.g. `and reg,0xFF` -> byte-clearing `xor`); a
// place for byte-granularity lowerings.  Runs before the accumulator pass.
FunctionPass *createI8086FixupByteMaskPass();

// Post-RA size peephole: ALU r/m,imm with an AL/AX destination -> the
// one-byte-shorter accumulator short form.
FunctionPass *createI8086CompactEncodingPass();

// Post-RA peephole: fold a 3-MOV register swap (through a scratch) into a single
// XCHG (preferring the 1-byte AX form), freeing the scratch register.
FunctionPass *createI8086SwapPeepholePass();

// Post-RA peephole: fold `add B,I; mov reg,[B]` into `mov reg,[B+I]` when the
// index is already in SI/DI and the base in BX/BP (no move/spill introduced).
FunctionPass *createI8086FoldIndexAddrPass();

// Post-RA peephole: reorder `mov R,[mem]; op R,imm` -> `mov R,imm; op R,[mem]`
// (commutative ops, R not AX/AL) when that is strictly smaller.
FunctionPass *createI8086FoldMemImmPass();

// Post-RA peephole: collapse a mov/add chain computing base+index+disp into one
// LEA (addressing-register operands only), when smaller.
FunctionPass *createI8086FormLeaPass();

void initializeI8086DAGToDAGISelLegacyPass(PassRegistry &);
void initializeI8086AsmPrinterPass(PassRegistry &);

} // namespace llvm

#endif // LLVM_LIB_TARGET_I8086_I8086_H
