//===-- I8086InstrInfo.td - Intel 8086 Instruction defs ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the full original Intel 8086 instruction set for the
// assembler (MC layer only).
//
//===----------------------------------------------------------------------===//

include "I8086InstrFormats.td"

//===----------------------------------------------------------------------===//
// Operand definitions.
//===----------------------------------------------------------------------===//

// Immediate operands: the built-in i8imm/i16imm (from Target.td) are reused.
// Width is chosen by the instruction's ImmType, not by the operand class.

// PC-relative branch targets (parsed as ordinary expressions/immediates).
def brtarget8  : Operand<i16> { let PrintMethod = "printPCRelImm"; }
def brtarget16 : Operand<i16> { let PrintMethod = "printPCRelImm"; }

// The literal "1" used by shift/rotate-by-one instructions.
def Imm1AsmOperand : AsmOperandClass {
  let Name = "Imm1";
  let PredicateMethod = "isImmOne";
  let RenderMethod = "addImmOperands";
}
def imm1 : Operand<i16> { let ParserMatchClass = Imm1AsmOperand; }

// Memory operands.  All expand to (base:GR16, index:GR16, disp:i16imm, seg:SEGR).
class I8086MemOperand<AsmOperandClass c> : Operand<i16> {
  let PrintMethod = "printMemReference";
  let MIOperandInfo = (ops GR16, GR16, i16imm, SEGR);
  let ParserMatchClass = c;
  let OperandType = "OPERAND_MEMORY";
}
def Mem8AsmOperand   : AsmOperandClass { let Name = "Mem8";   let RenderMethod = "addMemOperands"; let PredicateMethod = "isMem8"; }
def Mem16AsmOperand  : AsmOperandClass { let Name = "Mem16";  let RenderMethod = "addMemOperands"; let PredicateMethod = "isMem16"; }
def AnyMemAsmOperand : AsmOperandClass { let Name = "AnyMem"; let RenderMethod = "addMemOperands"; let PredicateMethod = "isMem"; }
// Explicitly-sized memory (requires "byte ptr"/"word ptr"): used where no other
// operand implies the access width.
def Mem8ExpAsmOperand  : AsmOperandClass { let Name = "Mem8Exp";  let RenderMethod = "addMemOperands"; let PredicateMethod = "isMem8Explicit"; }
def Mem16ExpAsmOperand : AsmOperandClass { let Name = "Mem16Exp"; let RenderMethod = "addMemOperands"; let PredicateMethod = "isMem16Explicit"; }
def mem8   : I8086MemOperand<Mem8AsmOperand>;
def mem16  : I8086MemOperand<Mem16AsmOperand>;
def anymem : I8086MemOperand<AnyMemAsmOperand>;
def mem8e  : I8086MemOperand<Mem8ExpAsmOperand>;
def mem16e : I8086MemOperand<Mem16ExpAsmOperand>;

//===----------------------------------------------------------------------===//
// Helper classes.
//===----------------------------------------------------------------------===//

// No-operand instruction (opcode byte only).
class NullaryInst<bits<8> op, string asm>
    : I8086Inst<op, RawFrm, NoImm, (outs), (ins), asm>;

//===----------------------------------------------------------------------===//
// Data transfer: MOV.
//===----------------------------------------------------------------------===//

// MOV r/m, reg  and  MOV reg, r/m
def MOV8mr  : I8086Inst<0x88, MRMDestMem, NoImm, (outs), (ins mem8:$dst, GR8:$src),   "mov\t$dst, $src">;
def MOV16mr : I8086Inst<0x89, MRMDestMem, NoImm, (outs), (ins mem16:$dst, GR16:$src), "mov\t$dst, $src">;
def MOV8rr  : I8086Inst<0x88, MRMDestReg, NoImm, (outs), (ins GR8:$dst, GR8:$src),    "mov\t$dst, $src">;
def MOV16rr : I8086Inst<0x89, MRMDestReg, NoImm, (outs), (ins GR16:$dst, GR16:$src),  "mov\t$dst, $src">;
def MOV8rm  : I8086Inst<0x8A, MRMSrcMem,  NoImm, (outs), (ins GR8:$dst, mem8:$src),   "mov\t$dst, $src">;
def MOV16rm : I8086Inst<0x8B, MRMSrcMem,  NoImm, (outs), (ins GR16:$dst, mem16:$src), "mov\t$dst, $src">;

// MOV reg, imm  (opcode B0+r / B8+r).
def MOV8ri  : I8086Inst<0xB0, AddRegFrm, Imm8,  (outs), (ins GR8:$dst, i8imm:$imm),   "mov\t$dst, $imm">;
def MOV16ri : I8086Inst<0xB8, AddRegFrm, Imm16, (outs), (ins GR16:$dst, i16imm:$imm), "mov\t$dst, $imm">;

// MOV r/m, imm  (opcode C6/C7 /0).
def MOV8mi  : I8086Inst<0xC6, MRM0m, Imm8,  (outs), (ins mem8e:$dst, i8imm:$imm),   "mov\t$dst, $imm">;
def MOV16mi : I8086Inst<0xC7, MRM0m, Imm16, (outs), (ins mem16e:$dst, i16imm:$imm), "mov\t$dst, $imm">;

// MOV involving segment registers (8C / 8E).
def MOV16rs : I8086Inst<0x8C, MRMDestReg, NoImm, (outs), (ins GR16:$dst, SEGR:$src), "mov\t$dst, $src">;
def MOV16ms : I8086Inst<0x8C, MRMDestMem, NoImm, (outs), (ins mem16:$dst, SEGR:$src),"mov\t$dst, $src">;
def MOVsr16 : I8086Inst<0x8E, MRMSrcReg,  NoImm, (outs), (ins SEGR:$dst, GR16:$src), "mov\t$dst, $src">;
def MOVsm16 : I8086Inst<0x8E, MRMSrcMem,  NoImm, (outs), (ins SEGR:$dst, mem16:$src),"mov\t$dst, $src">;

//===----------------------------------------------------------------------===//
// Data transfer: PUSH / POP / XCHG / IN / OUT / LEA / LDS / LES / XLAT.
//===----------------------------------------------------------------------===//

def PUSH16r  : I8086Inst<0x50, AddRegFrm, NoImm, (outs), (ins GR16:$reg), "push\t$reg">;
def PUSH16m  : I8086Inst<0xFF, MRM6m, NoImm, (outs), (ins mem16:$mem), "push\t$mem">;
def POP16r   : I8086Inst<0x58, AddRegFrm, NoImm, (outs), (ins GR16:$reg), "pop\t$reg">;
def POP16m   : I8086Inst<0x8F, MRM0m, NoImm, (outs), (ins mem16:$mem), "pop\t$mem">;

def PUSHES : NullaryInst<0x06, "push\tes">;
def PUSHCS : NullaryInst<0x0E, "push\tcs">;
def PUSHSS : NullaryInst<0x16, "push\tss">;
def PUSHDS : NullaryInst<0x1E, "push\tds">;
def POPES  : NullaryInst<0x07, "pop\tes">;
def POPSS  : NullaryInst<0x17, "pop\tss">;
def POPDS  : NullaryInst<0x1F, "pop\tds">;

def XCHG8rr  : I8086Inst<0x86, MRMDestReg, NoImm, (outs), (ins GR8:$dst, GR8:$src),   "xchg\t$dst, $src">;
def XCHG16rr : I8086Inst<0x87, MRMDestReg, NoImm, (outs), (ins GR16:$dst, GR16:$src), "xchg\t$dst, $src">;
def XCHG8rm  : I8086Inst<0x86, MRMDestMem, NoImm, (outs), (ins mem8:$dst, GR8:$src),  "xchg\t$src, $dst">;
def XCHG16rm : I8086Inst<0x87, MRMDestMem, NoImm, (outs), (ins mem16:$dst, GR16:$src),"xchg\t$src, $dst">;

def IN8ri  : I8086Inst<0xE4, RawFrm, Imm8, (outs), (ins AREG8:$dst, i8imm:$port),  "in\t$dst, $port">;
def IN16ri : I8086Inst<0xE5, RawFrm, Imm8, (outs), (ins AREG16:$dst, i8imm:$port), "in\t$dst, $port">;
def IN8rr  : I8086Inst<0xEC, RawFrm, NoImm, (outs), (ins AREG8:$dst, DREG16:$dx),  "in\t$dst, $dx">;
def IN16rr : I8086Inst<0xED, RawFrm, NoImm, (outs), (ins AREG16:$dst, DREG16:$dx), "in\t$dst, $dx">;
def OUT8ri  : I8086Inst<0xE6, RawFrm, Imm8, (outs), (ins AREG8:$src, i8imm:$port),  "out\t$port, $src">;
def OUT16ri : I8086Inst<0xE7, RawFrm, Imm8, (outs), (ins AREG16:$src, i8imm:$port), "out\t$port, $src">;
def OUT8rr  : I8086Inst<0xEE, RawFrm, NoImm, (outs), (ins DREG16:$dx, AREG8:$src),  "out\t$dx, $src">;
def OUT16rr : I8086Inst<0xEF, RawFrm, NoImm, (outs), (ins DREG16:$dx, AREG16:$src), "out\t$dx, $src">;

def LEA16 : I8086Inst<0x8D, MRMSrcMem, NoImm, (outs), (ins GR16:$dst, anymem:$src), "lea\t$dst, $src">;
def LDS16 : I8086Inst<0xC5, MRMSrcMem, NoImm, (outs), (ins GR16:$dst, anymem:$src), "lds\t$dst, $src">;
def LES16 : I8086Inst<0xC4, MRMSrcMem, NoImm, (outs), (ins GR16:$dst, anymem:$src), "les\t$dst, $src">;

def LAHF  : NullaryInst<0x9F, "lahf">;
def SAHF  : NullaryInst<0x9E, "sahf">;
def PUSHF : NullaryInst<0x9C, "pushf">;
def POPF  : NullaryInst<0x9D, "popf">;
def XLAT  : NullaryInst<0xD7, "xlat">;

//===----------------------------------------------------------------------===//
// Arithmetic / logic core group (ADD OR ADC SBB AND SUB XOR CMP).
//===----------------------------------------------------------------------===//

multiclass ArithLogic<string m, bits<8> base, Format ri, Format mi> {
  def 8rr  : I8086Inst<!add(base,0), MRMDestReg, NoImm, (outs), (ins GR8:$dst, GR8:$src),    m#"\t$dst, $src">;
  def 16rr : I8086Inst<!add(base,1), MRMDestReg, NoImm, (outs), (ins GR16:$dst, GR16:$src),  m#"\t$dst, $src">;
  def 8mr  : I8086Inst<!add(base,0), MRMDestMem, NoImm, (outs), (ins mem8:$dst, GR8:$src),   m#"\t$dst, $src">;
  def 16mr : I8086Inst<!add(base,1), MRMDestMem, NoImm, (outs), (ins mem16:$dst, GR16:$src), m#"\t$dst, $src">;
  def 8rm  : I8086Inst<!add(base,2), MRMSrcMem,  NoImm, (outs), (ins GR8:$dst, mem8:$src),   m#"\t$dst, $src">;
  def 16rm : I8086Inst<!add(base,3), MRMSrcMem,  NoImm, (outs), (ins GR16:$dst, mem16:$src), m#"\t$dst, $src">;
  def 8ri  : I8086Inst<0x80, ri, Imm8,  (outs), (ins GR8:$dst, i8imm:$imm),   m#"\t$dst, $imm">;
  def 16ri : I8086Inst<0x81, ri, Imm16, (outs), (ins GR16:$dst, i16imm:$imm), m#"\t$dst, $imm">;
  def 8mi  : I8086Inst<0x80, mi, Imm8,  (outs), (ins mem8e:$dst, i8imm:$imm),   m#"\t$dst, $imm">;
  def 16mi : I8086Inst<0x81, mi, Imm16, (outs), (ins mem16e:$dst, i16imm:$imm), m#"\t$dst, $imm">;
}

defm ADD : ArithLogic<"add", 0x00, MRM0r, MRM0m>;
defm OR  : ArithLogic<"or",  0x08, MRM1r, MRM1m>;
defm ADC : ArithLogic<"adc", 0x10, MRM2r, MRM2m>;
defm SBB : ArithLogic<"sbb", 0x18, MRM3r, MRM3m>;
defm AND : ArithLogic<"and", 0x20, MRM4r, MRM4m>;
defm SUB : ArithLogic<"sub", 0x28, MRM5r, MRM5m>;
defm XOR : ArithLogic<"xor", 0x30, MRM6r, MRM6m>;
defm CMP : ArithLogic<"cmp", 0x38, MRM7r, MRM7m>;

// TEST (group differs: 84/85 for r/m,reg; F6/F7 /0 for r/m,imm).
def TEST8rr  : I8086Inst<0x84, MRMDestReg, NoImm, (outs), (ins GR8:$dst, GR8:$src),   "test\t$dst, $src">;
def TEST16rr : I8086Inst<0x85, MRMDestReg, NoImm, (outs), (ins GR16:$dst, GR16:$src), "test\t$dst, $src">;
def TEST8mr  : I8086Inst<0x84, MRMDestMem, NoImm, (outs), (ins mem8:$dst, GR8:$src),  "test\t$dst, $src">;
def TEST16mr : I8086Inst<0x85, MRMDestMem, NoImm, (outs), (ins mem16:$dst, GR16:$src),"test\t$dst, $src">;
def TEST8ri  : I8086Inst<0xF6, MRM0r, Imm8,  (outs), (ins GR8:$dst, i8imm:$imm),   "test\t$dst, $imm">;
def TEST16ri : I8086Inst<0xF7, MRM0r, Imm16, (outs), (ins GR16:$dst, i16imm:$imm), "test\t$dst, $imm">;
def TEST8mi  : I8086Inst<0xF6, MRM0m, Imm8,  (outs), (ins mem8e:$dst, i8imm:$imm),   "test\t$dst, $imm">;
def TEST16mi : I8086Inst<0xF7, MRM0m, Imm16, (outs), (ins mem16e:$dst, i16imm:$imm), "test\t$dst, $imm">;

//===----------------------------------------------------------------------===//
// INC / DEC / NOT / NEG / MUL / IMUL / DIV / IDIV.
//===----------------------------------------------------------------------===//

def INC16r : I8086Inst<0x40, AddRegFrm, NoImm, (outs), (ins GR16:$reg), "inc\t$reg">;
def DEC16r : I8086Inst<0x48, AddRegFrm, NoImm, (outs), (ins GR16:$reg), "dec\t$reg">;
def INC8r  : I8086Inst<0xFE, MRM0r, NoImm, (outs), (ins GR8:$reg),  "inc\t$reg">;
def DEC8r  : I8086Inst<0xFE, MRM1r, NoImm, (outs), (ins GR8:$reg),  "dec\t$reg">;
def INC8m  : I8086Inst<0xFE, MRM0m, NoImm, (outs), (ins mem8e:$mem), "inc\t$mem">;
def DEC8m  : I8086Inst<0xFE, MRM1m, NoImm, (outs), (ins mem8e:$mem), "dec\t$mem">;
def INC16m : I8086Inst<0xFF, MRM0m, NoImm, (outs), (ins mem16e:$mem), "inc\t$mem">;
def DEC16m : I8086Inst<0xFF, MRM1m, NoImm, (outs), (ins mem16e:$mem), "dec\t$mem">;

// Group 3 (F6/F7): NOT=/2 NEG=/3 MUL=/4 IMUL=/5 DIV=/6 IDIV=/7.
multiclass Grp3<string m, Format r8, Format m8, Format r16, Format m16> {
  def 8r  : I8086Inst<0xF6, r8,  NoImm, (outs), (ins GR8:$reg),   m#"\t$reg">;
  def 8m  : I8086Inst<0xF6, m8,  NoImm, (outs), (ins mem8e:$mem),  m#"\t$mem">;
  def 16r : I8086Inst<0xF7, r16, NoImm, (outs), (ins GR16:$reg),  m#"\t$reg">;
  def 16m : I8086Inst<0xF7, m16, NoImm, (outs), (ins mem16e:$mem), m#"\t$mem">;
}
defm NOT  : Grp3<"not",  MRM2r, MRM2m, MRM2r, MRM2m>;
defm NEG  : Grp3<"neg",  MRM3r, MRM3m, MRM3r, MRM3m>;
defm MUL  : Grp3<"mul",  MRM4r, MRM4m, MRM4r, MRM4m>;
defm IMUL : Grp3<"imul", MRM5r, MRM5m, MRM5r, MRM5m>;
defm DIV  : Grp3<"div",  MRM6r, MRM6m, MRM6r, MRM6m>;
defm IDIV : Grp3<"idiv", MRM7r, MRM7m, MRM7r, MRM7m>;

// Decimal / ASCII adjust and sign extension.
def DAA : NullaryInst<0x27, "daa">;
def DAS : NullaryInst<0x2F, "das">;
def AAA : NullaryInst<0x37, "aaa">;
def AAS : NullaryInst<0x3F, "aas">;
def AAM : I8086Inst<0xD4, RawFrm, Imm8, (outs), (ins i8imm:$base), "aam\t$base">;
def AAD : I8086Inst<0xD5, RawFrm, Imm8, (outs), (ins i8imm:$base), "aad\t$base">;
def CBW : NullaryInst<0x98, "cbw">;
def CWD : NullaryInst<0x99, "cwd">;

//===----------------------------------------------------------------------===//
// Shifts and rotates (group 2: D0/D1 by 1, D2/D3 by CL).
//   ROL=/0 ROR=/1 RCL=/2 RCR=/3 SHL=/4 SHR=/5 SAR=/7
//===----------------------------------------------------------------------===//

multiclass ShiftRotate<string m, Format r, Format mm> {
  def 8r1  : I8086Inst<0xD0, r,  NoImm, (outs), (ins GR8:$dst, imm1:$one),   m#"\t$dst, $one">;
  def 16r1 : I8086Inst<0xD1, r,  NoImm, (outs), (ins GR16:$dst, imm1:$one),  m#"\t$dst, $one">;
  def 8m1  : I8086Inst<0xD0, mm, NoImm, (outs), (ins mem8e:$dst, imm1:$one),  m#"\t$dst, $one">;
  def 16m1 : I8086Inst<0xD1, mm, NoImm, (outs), (ins mem16e:$dst, imm1:$one), m#"\t$dst, $one">;
  def 8rCL  : I8086Inst<0xD2, r,  NoImm, (outs), (ins GR8:$dst, CREG8:$cl),   m#"\t$dst, $cl">;
  def 16rCL : I8086Inst<0xD3, r,  NoImm, (outs), (ins GR16:$dst, CREG8:$cl),  m#"\t$dst, $cl">;
  def 8mCL  : I8086Inst<0xD2, mm, NoImm, (outs), (ins mem8e:$dst, CREG8:$cl),  m#"\t$dst, $cl">;
  def 16mCL : I8086Inst<0xD3, mm, NoImm, (outs), (ins mem16e:$dst, CREG8:$cl), m#"\t$dst, $cl">;
}
defm ROL : ShiftRotate<"rol", MRM0r, MRM0m>;
defm ROR : ShiftRotate<"ror", MRM1r, MRM1m>;
defm RCL : ShiftRotate<"rcl", MRM2r, MRM2m>;
defm RCR : ShiftRotate<"rcr", MRM3r, MRM3m>;
defm SHL : ShiftRotate<"shl", MRM4r, MRM4m>;
defm SHR : ShiftRotate<"shr", MRM5r, MRM5m>;
defm SAR : ShiftRotate<"sar", MRM7r, MRM7m>;

//===----------------------------------------------------------------------===//
// String instructions and prefixes.
//===----------------------------------------------------------------------===//

def MOVSB : NullaryInst<0xA4, "movsb">;
def MOVSW : NullaryInst<0xA5, "movsw">;
def CMPSB : NullaryInst<0xA6, "cmpsb">;
def CMPSW : NullaryInst<0xA7, "cmpsw">;
def STOSB : NullaryInst<0xAA, "stosb">;
def STOSW : NullaryInst<0xAB, "stosw">;
def LODSB : NullaryInst<0xAC, "lodsb">;
def LODSW : NullaryInst<0xAD, "lodsw">;
def SCASB : NullaryInst<0xAE, "scasb">;
def SCASW : NullaryInst<0xAF, "scasw">;

// Prefix bytes emitted as standalone instructions and combined by the parser.
let isCodeGenOnly = 1 in {
  def REP_PREFIX   : NullaryInst<0xF3, "rep">;
  def REPNE_PREFIX : NullaryInst<0xF2, "repne">;
  def LOCK_PREFIX  : NullaryInst<0xF0, "lock">;
}

//===----------------------------------------------------------------------===//
// Control transfer.
//===----------------------------------------------------------------------===//

// Unconditional jumps.
def JMP16    : I8086Inst<0xE9, RawFrm, Rel16, (outs), (ins brtarget16:$dst), "jmp\t$dst">;
def JMP8     : I8086Inst<0xEB, RawFrm, Rel8,  (outs), (ins brtarget8:$dst),  "jmp\t$dst">;
def JMP16r   : I8086Inst<0xFF, MRM4r, NoImm, (outs), (ins GR16:$dst),  "jmp\t$dst">;
def JMP16m   : I8086Inst<0xFF, MRM4m, NoImm, (outs), (ins mem16:$dst), "jmp\t$dst">;
let isCodeGenOnly = 1 in
def JMPFAR   : I8086Inst<0xEA, RawFrmFar, NoImm, (outs), (ins i16imm:$off, i16imm:$seg), "jmp\t$seg:$off">;

def CALL16   : I8086Inst<0xE8, RawFrm, Rel16, (outs), (ins brtarget16:$dst), "call\t$dst">;
def CALL16r  : I8086Inst<0xFF, MRM2r, NoImm, (outs), (ins GR16:$dst),  "call\t$dst">;
def CALL16m  : I8086Inst<0xFF, MRM2m, NoImm, (outs), (ins mem16:$dst), "call\t$dst">;
let isCodeGenOnly = 1 in
def CALLFAR  : I8086Inst<0x9A, RawFrmFar, NoImm, (outs), (ins i16imm:$off, i16imm:$seg), "call\t$seg:$off">;

def RET      : NullaryInst<0xC3, "ret">;
def RETI     : I8086Inst<0xC2, RawFrm, Imm16, (outs), (ins i16imm:$amt), "ret\t$amt">;
def RETF     : NullaryInst<0xCB, "retf">;
def RETFI    : I8086Inst<0xCA, RawFrm, Imm16, (outs), (ins i16imm:$amt), "retf\t$amt">;

// Conditional jumps (all rel8 on the 8086).
class Jcc<bits<8> op, string m>
    : I8086Inst<op, RawFrm, Rel8, (outs), (ins brtarget8:$dst), m#"\t$dst">;
def JO   : Jcc<0x70, "jo">;
def JNO  : Jcc<0x71, "jno">;
def JB   : Jcc<0x72, "jb">;
def JAE  : Jcc<0x73, "jae">;
def JE   : Jcc<0x74, "je">;
def JNE  : Jcc<0x75, "jne">;
def JBE  : Jcc<0x76, "jbe">;
def JA   : Jcc<0x77, "ja">;
def JS   : Jcc<0x78, "js">;
def JNS  : Jcc<0x79, "jns">;
def JP   : Jcc<0x7A, "jp">;
def JNP  : Jcc<0x7B, "jnp">;
def JL   : Jcc<0x7C, "jl">;
def JGE  : Jcc<0x7D, "jge">;
def JLE  : Jcc<0x7E, "jle">;
def JG   : Jcc<0x7F, "jg">;

def LOOP   : I8086Inst<0xE2, RawFrm, Rel8, (outs), (ins brtarget8:$dst), "loop\t$dst">;
def LOOPE  : I8086Inst<0xE1, RawFrm, Rel8, (outs), (ins brtarget8:$dst), "loope\t$dst">;
def LOOPNE : I8086Inst<0xE0, RawFrm, Rel8, (outs), (ins brtarget8:$dst), "loopne\t$dst">;
def JCXZ   : I8086Inst<0xE3, RawFrm, Rel8, (outs), (ins brtarget8:$dst), "jcxz\t$dst">;

def INT   : I8086Inst<0xCD, RawFrm, Imm8, (outs), (ins i8imm:$vec), "int\t$vec">;
def INT3  : NullaryInst<0xCC, "int3">;
def INTO  : NullaryInst<0xCE, "into">;
def IRET  : NullaryInst<0xCF, "iret">;

//===----------------------------------------------------------------------===//
// Flag and processor control.
//===----------------------------------------------------------------------===//

def CLC : NullaryInst<0xF8, "clc">;
def STC : NullaryInst<0xF9, "stc">;
def CLI : NullaryInst<0xFA, "cli">;
def STI : NullaryInst<0xFB, "sti">;
def CLD : NullaryInst<0xFC, "cld">;
def STD : NullaryInst<0xFD, "std">;
def CMC : NullaryInst<0xF5, "cmc">;

def NOP  : NullaryInst<0x90, "nop">;
def HLT  : NullaryInst<0xF4, "hlt">;
def WAIT : NullaryInst<0x9B, "wait">;
