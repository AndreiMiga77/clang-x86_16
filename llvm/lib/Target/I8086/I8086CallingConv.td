//===-- I8086CallingConv.td - Calling Conventions for i8086 -*- tablegen -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// 16-bit "tiny model" cdecl: all arguments are passed on the stack, pushed
// right-to-left, and the caller cleans up.  Integer results are returned in the
// accumulator (AL / AX), wider results in register pairs (DX:AX for 32-bit,
// then CX, BX for 64-bit).  Structures are returned via a hidden sret pointer.
//
//===----------------------------------------------------------------------===//

def RetCC_I8086 : CallingConv<[
  // i8 in AL; i16/pointers in AX; i32 as (AX=lo, DX=hi); i64 as
  // (AX, DX, CX, BX) after legalization splits it into i16 parts.
  CCIfType<[i8],  CCAssignToReg<[AL]>>,
  CCIfType<[i16], CCAssignToReg<[AX, DX, CX, BX]>>
]>;

def CC_I8086 : CallingConv<[
  // Structures returned by value use a hidden pointer (near, 16-bit).
  CCIfSRet<CCIfType<[i16], CCAssignToStack<2, 2>>>,

  // Byval aggregates are passed on the stack.
  CCIfByVal<CCPassByVal<2, 2>>,

  // Promote i8 arguments to i16 (word-sized stack slots).
  CCIfType<[i8], CCPromoteToType<i16>>,

  // Everything is passed in 2-byte, 2-byte-aligned stack slots.
  CCIfType<[i16], CCAssignToStack<2, 2>>
]>;

// Callee-saved registers for the cdecl convention.
def CSR_I8086 : CalleeSavedRegs<(add SI, DI, BP)>;
