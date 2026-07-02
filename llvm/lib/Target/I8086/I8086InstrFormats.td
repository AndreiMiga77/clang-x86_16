//===-- I8086InstrFormats.td - I8086 Instruction Formats ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Describes the encoding "form" of each 8086 instruction.  The form, immediate
// kind and opcode byte are packed into TSFlags and consumed by the hand-written
// I8086MCCodeEmitter.  Keep the numeric values in sync with I8086BaseInfo.h.
//
//===----------------------------------------------------------------------===//

// Encoding form.  Values must match I8086::Form in I8086BaseInfo.h.
class Format<bits<6> val> { bits<6> Value = val; }
def Pseudo     : Format<0>;
def RawFrm     : Format<1>;  // opcode only (+ optional trailing immediate)
def AddRegFrm  : Format<2>;  // opcode | reg(op0)  (+ optional trailing imm)
def MRMDestReg : Format<3>;  // ModRM: rm=op0(reg), reg=op1(reg)
def MRMDestMem : Format<4>;  // ModRM: rm=mem(op0..3), reg=op4(reg)
def MRMSrcReg  : Format<5>;  // ModRM: reg=op0(reg), rm=op1(reg)
def MRMSrcMem  : Format<6>;  // ModRM: reg=op0(reg), rm=mem(op1..4)
def RawFrmFar  : Format<7>;  // opcode + imm16(off) + imm16(seg)  (far ptr)
// Group forms: reg field is a fixed /digit extension.
def MRM0r : Format<8>;   def MRM1r : Format<9>;
def MRM2r : Format<10>;  def MRM3r : Format<11>;
def MRM4r : Format<12>;  def MRM5r : Format<13>;
def MRM6r : Format<14>;  def MRM7r : Format<15>;
def MRM0m : Format<16>;  def MRM1m : Format<17>;
def MRM2m : Format<18>;  def MRM3m : Format<19>;
def MRM4m : Format<20>;  def MRM5m : Format<21>;
def MRM6m : Format<22>;  def MRM7m : Format<23>;

// Immediate kind.  Values must match I8086::ImmType in I8086BaseInfo.h.
class ImmType<bits<3> val> { bits<3> Value = val; }
def NoImm : ImmType<0>;
def Imm8  : ImmType<1>;  // 8-bit absolute immediate
def Imm16 : ImmType<2>;  // 16-bit absolute immediate
def Rel8  : ImmType<3>;  // 8-bit pc-relative branch target
def Rel16 : ImmType<4>;  // 16-bit pc-relative branch target

class I8086Inst<bits<8> opcod, Format f, ImmType i, dag outs, dag ins,
                string asmstr, list<dag> pattern = []> : Instruction {
  let Namespace = "I8086";

  bits<8> Opcode = opcod;
  Format  Form   = f;
  ImmType ImmT   = i;

  let OutOperandList = outs;
  let InOperandList  = ins;
  let AsmString      = asmstr;
  let Pattern        = pattern;

  // Variable length; the emitter computes the real size.
  let Size = 0;

  let TSFlags{5-0}  = Form.Value;
  let TSFlags{8-6}  = ImmT.Value;
  let TSFlags{16-9} = Opcode;
}

// Pseudo instructions (expanded before or during emission).
class I8086Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern = []>
    : I8086Inst<0, Pseudo, NoImm, outs, ins, asmstr, pattern> {
  let isPseudo = 1;
  let hasSideEffects = 0;
}
