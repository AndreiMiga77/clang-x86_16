//===-- I8086RegisterInfo.td - I8086 Register defs ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//  Declarations that describe the Intel 8086 register file.
//===----------------------------------------------------------------------===//

class I8086Reg<string n, bits<16> Enc> : Register<n> {
  let Namespace = "I8086";
  let HWEncoding = Enc;
}

// Sub-register indices for the 8-bit halves of the general 16-bit registers.
let Namespace = "I8086" in {
  def sub_8bit_lo : SubRegIndex<8, 0>;
  def sub_8bit_hi : SubRegIndex<8, 8>;
}

// 8-bit general purpose registers.  Encoding is the ModR/M reg/rm field value.
def AL : I8086Reg<"al", 0>;
def CL : I8086Reg<"cl", 1>;
def DL : I8086Reg<"dl", 2>;
def BL : I8086Reg<"bl", 3>;
def AH : I8086Reg<"ah", 4>;
def CH : I8086Reg<"ch", 5>;
def DH : I8086Reg<"dh", 6>;
def BH : I8086Reg<"bh", 7>;

// 16-bit general purpose registers, composed of their 8-bit halves where
// applicable.
let SubRegIndices = [sub_8bit_lo, sub_8bit_hi], CoveredBySubRegs = 1 in {
  def AX : I8086Reg<"ax", 0> { let SubRegs = [AL, AH]; }
  def CX : I8086Reg<"cx", 1> { let SubRegs = [CL, CH]; }
  def DX : I8086Reg<"dx", 2> { let SubRegs = [DL, DH]; }
  def BX : I8086Reg<"bx", 3> { let SubRegs = [BL, BH]; }
}
def SP : I8086Reg<"sp", 4>;
def BP : I8086Reg<"bp", 5>;
def SI : I8086Reg<"si", 6>;
def DI : I8086Reg<"di", 7>;

// Segment registers.  Encoding is the sreg field value.
def ES : I8086Reg<"es", 0>;
def CS : I8086Reg<"cs", 1>;
def SS : I8086Reg<"ss", 2>;
def DS : I8086Reg<"ds", 3>;

// Instruction pointer and flags (not directly addressable as operands).
def IP    : I8086Reg<"ip", 0>;
def FLAGS : I8086Reg<"flags", 0>;

//===----------------------------------------------------------------------===//
// Register classes.
//===----------------------------------------------------------------------===//

def GR8 : RegisterClass<"I8086", [i8], 8,
                        (add AL, CL, DL, BL, AH, CH, DH, BH)>;

// Allocation order: caller-saved first (AX, CX, DX, BX), then callee-saved
// (SI, DI); BP and SP come last and are reserved via getReservedRegs.
def GR16 : RegisterClass<"I8086", [i16], 16,
                         (add AX, CX, DX, BX, SI, DI, BP, SP)>;

def SEGR : RegisterClass<"I8086", [i16], 16, (add ES, CS, SS, DS)>;

// Registers usable as a ModR/M base/index on the 8086.  Only BX, BP, SI and DI
// can address memory; BP is reserved (frame pointer) but appears here so that
// frame-index memory operands (rewritten to BP by eliminateFrameIndex) satisfy
// the operand's register class.
def ADDR16 : RegisterClass<"I8086", [i16], 16, (add BX, SI, DI, BP)>;

// Two-register [base+index] addressing (8086 allows only base in {BX,BP} and
// index in {SI,DI}).  These constrain only the operands of the indexed
// load/store forms; plain single-register addressing keeps the flexible ADDR16
// so it is unaffected.
def BASE16 : RegisterClass<"I8086", [i16], 16, (add BX, BP)>;
def IDX16  : RegisterClass<"I8086", [i16], 16, (add SI, DI)>;

// Single-register sub-classes for instructions with fixed register operands
// (accumulator for IN/OUT, CL for shifts, DX for variable-port IN/OUT).
def AREG8  : RegisterClass<"I8086", [i8],  8,  (add AL)>;
def AREG16 : RegisterClass<"I8086", [i16], 16, (add AX)>;
def CREG8  : RegisterClass<"I8086", [i8],  8,  (add CL)>;
def DREG16 : RegisterClass<"I8086", [i16], 16, (add DX)>;
