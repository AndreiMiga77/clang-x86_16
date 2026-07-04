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

// CodeGen basic-block branch target (OtherVT so it accepts an MBB).  The asm
// parser still supplies an immediate/expression, accepted via this class.
def BrTargetAsmOperand : AsmOperandClass {
  let Name = "BrTarget";
  let RenderMethod = "addImmOperands";
  let PredicateMethod = "isImm";
}
def jmptarget : Operand<OtherVT> {
  let PrintMethod = "printPCRelImm";
  let ParserMatchClass = BrTargetAsmOperand;
}

// The literal "1" used by shift/rotate-by-one instructions.
def Imm1AsmOperand : AsmOperandClass {
  let Name = "Imm1";
  let PredicateMethod = "isImmOne";
  let RenderMethod = "addImmOperands";
}
def imm1 : Operand<i16> { let ParserMatchClass = Imm1AsmOperand; }

// A 16-bit immediate that fits in a sign-extended byte, for the compact 0x83
// group (`add/or/adc/sbb/and/sub/xor/cmp r/m16, imm8`).  The AsmOperandClass is
// a subclass of the generic immediate class so the assembler prefers this
// (shorter) form when the value fits; larger values fall back to the imm16 form.
def i16immSExt8 : ImmLeaf<i16, [{ return isInt<8>(Imm); }]>;
def Imm16i8AsmOperand : AsmOperandClass {
  let Name = "Imm16i8";
  let PredicateMethod = "isImmSExti16i8";
  let RenderMethod = "addImmOperands";
  let SuperClasses = [ImmAsmOperand];
}
def i16i8imm : Operand<i16> { let ParserMatchClass = Imm16i8AsmOperand; }

// Memory operands.  All expand to (base:GR16, index:GR16, disp:i16imm, seg:SEGR).
class I8086MemOperand<AsmOperandClass c> : Operand<i16> {
  let PrintMethod = "printMemReference";
  // base and index must be ModR/M-addressable registers (BX/BP/SI/DI).
  let MIOperandInfo = (ops ADDR16, ADDR16, i16imm, SEGR);
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

// A bare direct address `[disp16]` for the accumulator moffs forms.  Modeled as
// a full memory operand (same base/index/disp/seg layout and addMemOperands
// rendering as the ordinary mem operands) so it is a proper subclass of the
// Mem{8,16} classes -- the assembler then prefers the shorter moffs form -- and
// so the codegen rewrite is a plain in-place opcode swap.  Only the 16-bit
// displacement is encoded; RawFrm emits no ModRM.
def AbsMem8AsmOperand  : AsmOperandClass {
  let Name = "AbsMem8";  let PredicateMethod = "isAbsMem8";
  let RenderMethod = "addMemOperands"; let SuperClasses = [Mem8AsmOperand];
}
def AbsMem16AsmOperand : AsmOperandClass {
  let Name = "AbsMem16"; let PredicateMethod = "isAbsMem16";
  let RenderMethod = "addMemOperands"; let SuperClasses = [Mem16AsmOperand];
}
def absmem8  : I8086MemOperand<AbsMem8AsmOperand>;
def absmem16 : I8086MemOperand<AbsMem16AsmOperand>;
def mem8   : I8086MemOperand<Mem8AsmOperand>;
def mem16  : I8086MemOperand<Mem16AsmOperand>;
def anymem : I8086MemOperand<AnyMemAsmOperand>;
def mem8e  : I8086MemOperand<Mem8ExpAsmOperand>;
def mem16e : I8086MemOperand<Mem16ExpAsmOperand>;

// CodeGen-only two-register [base+index] operand: base in {BX,BP}, index in
// {SI,DI} (the only legal 8086 combos).  Same base/index/disp/seg layout as the
// ordinary mem operands, so the emitter, sizer and cost model handle it as-is.
def memidx : Operand<i16> {
  let PrintMethod = "printMemReference";
  let MIOperandInfo = (ops BASE16, IDX16, i16imm, SEGR);
  let OperandType = "OPERAND_MEMORY";
}

//===----------------------------------------------------------------------===//
// SelectionDAG node definitions (for CodeGen; the -gen-sd-node-info backend
// generates the I8086ISD:: enum from these).
//===----------------------------------------------------------------------===//

def SDT_I8086Call         : SDTypeProfile<0, -1, [SDTCisVT<0, iPTR>]>;
def SDT_I8086CallSeqStart : SDCallSeqStart<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;
def SDT_I8086CallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;
def SDT_I8086Wrapper      : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>,
                                                 SDTCisPtrTy<0>]>;
def SDT_I8086Cmp          : SDTypeProfile<0, 2, [SDTCisSameAs<0, 1>]>;
def SDT_I8086BrCC         : SDTypeProfile<0, 2, [SDTCisVT<0, OtherVT>,
                                                 SDTCisVT<1, i8>]>;
def SDT_I8086SelectCC     : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                                 SDTCisSameAs<1, 2>,
                                                 SDTCisVT<3, i8>]>;
def SDT_I8086Push         : SDTypeProfile<0, 1, [SDTCisVT<0, i16>]>;

def I8086retglue : SDNode<"I8086ISD::RET_GLUE", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def I8086call : SDNode<"I8086ISD::CALL", SDT_I8086Call,
                       [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue, SDNPVariadic]>;
def I8086callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_I8086CallSeqStart,
                                [SDNPHasChain, SDNPOutGlue]>;
def I8086callseq_end : SDNode<"ISD::CALLSEQ_END", SDT_I8086CallSeqEnd,
                              [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def I8086Wrapper : SDNode<"I8086ISD::Wrapper", SDT_I8086Wrapper>;
def I8086cmp : SDNode<"I8086ISD::CMP", SDT_I8086Cmp, [SDNPOutGlue]>;
def I8086brcc : SDNode<"I8086ISD::BR_CC", SDT_I8086BrCC,
                       [SDNPHasChain, SDNPInGlue]>;
def I8086selectcc : SDNode<"I8086ISD::SELECT_CC", SDT_I8086SelectCC,
                           [SDNPInGlue]>;
// Push a 16-bit value (used to pass outgoing stack arguments, since SP is not
// a ModR/M base register on the 8086).
def I8086push : SDNode<"I8086ISD::PUSH", SDT_I8086Push,
                       [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue]>;

// 16-bit addressing mode: (base:GR16, index:GR16, disp:i16imm, seg:SEGR),
// matched by I8086DAGToDAGISel::selectMemAddr.  frameindex is matched too so
// stack slots fold into memory operands.
def addr : ComplexPattern<i16, 4, "selectMemAddr", [frameindex], []>;

//===----------------------------------------------------------------------===//
// Helper classes.
//===----------------------------------------------------------------------===//

// No-operand instruction (opcode byte only).
class NullaryInst<bits<8> op, string asm>
    : I8086Inst<op, RawFrm, NoImm, (outs), (ins), asm>;

//===----------------------------------------------------------------------===//
// Data transfer: MOV.
//===----------------------------------------------------------------------===//

// MOV reg, reg (register copy).
def MOV8rr  : I8086Inst<0x88, MRMDestReg, NoImm, (outs GR8:$dst),  (ins GR8:$src),  "mov\t$dst, $src">;
def MOV16rr : I8086Inst<0x89, MRMDestReg, NoImm, (outs GR16:$dst), (ins GR16:$src), "mov\t$dst, $src">;

// MOV r/m, reg (store).
def MOV8mr  : I8086Inst<0x88, MRMDestMem, NoImm, (outs), (ins mem8:$dst, GR8:$src),
                        "mov\t$dst, $src", [(store GR8:$src, addr:$dst)]>;
def MOV16mr : I8086Inst<0x89, MRMDestMem, NoImm, (outs), (ins mem16:$dst, GR16:$src),
                        "mov\t$dst, $src", [(store GR16:$src, addr:$dst)]>;

// MOV reg, r/m (load).
def MOV8rm  : I8086Inst<0x8A, MRMSrcMem, NoImm, (outs GR8:$dst),  (ins mem8:$src),
                        "mov\t$dst, $src", [(set GR8:$dst, (load addr:$src))]>;
def MOV16rm : I8086Inst<0x8B, MRMSrcMem, NoImm, (outs GR16:$dst), (ins mem16:$src),
                        "mov\t$dst, $src", [(set GR16:$dst, (load addr:$src))]>;

// Two-register [base+index] load/store (CodeGen-only, no isel pattern).  Folding
// address arithmetic into an indexed operand is a register-allocation decision
// (it is only a win when the index is already in a callee-saved SI/DI, so no
// move/spill is introduced), which isel cannot make -- so these are produced
// solely by the post-RA I8086FoldIndexAddr pass.  Same encoding as the plain MOV
// load/store forms; the memidx operand's BASE16/IDX16 classes document the only
// legal 8086 base+index pairing.
let isCodeGenOnly = 1 in {
let mayStore = 1 in {
def MOV8mr_idx  : I8086Inst<0x88, MRMDestMem, NoImm, (outs), (ins memidx:$dst, GR8:$src),  "mov\t$dst, $src">;
def MOV16mr_idx : I8086Inst<0x89, MRMDestMem, NoImm, (outs), (ins memidx:$dst, GR16:$src), "mov\t$dst, $src">;
}
let mayLoad = 1 in {
def MOV8rm_idx  : I8086Inst<0x8A, MRMSrcMem, NoImm, (outs GR8:$dst),  (ins memidx:$src), "mov\t$dst, $src">;
def MOV16rm_idx : I8086Inst<0x8B, MRMSrcMem, NoImm, (outs GR16:$dst), (ins memidx:$src), "mov\t$dst, $src">;
}
}

// Accumulator <-> direct memory (moffs): AL/AX to/from a bare [disp16] address.
// 3 bytes (opcode + disp16) versus 4 for the ModRM form; selected for codegen by
// the I8086CompactEncoding pass and preferred by the assembler.
let mayLoad = 1 in {
  def MOV8ao  : I8086Inst<0xA0, RawFrm, Imm16, (outs AREG8:$dst),  (ins absmem8:$src),  "mov\t$dst, $src">;
  def MOV16ao : I8086Inst<0xA1, RawFrm, Imm16, (outs AREG16:$dst), (ins absmem16:$src), "mov\t$dst, $src">;
}
let mayStore = 1 in {
  def MOV8oa  : I8086Inst<0xA2, RawFrm, Imm16, (outs), (ins absmem8:$dst,  AREG8:$src),  "mov\t$dst, $src">;
  def MOV16oa : I8086Inst<0xA3, RawFrm, Imm16, (outs), (ins absmem16:$dst, AREG16:$src), "mov\t$dst, $src">;
}

// MOV reg, imm  (opcode B0+r / B8+r).
def MOV8ri  : I8086Inst<0xB0, AddRegFrm, Imm8,  (outs GR8:$dst),  (ins i8imm:$imm),
                        "mov\t$dst, $imm", [(set GR8:$dst, imm:$imm)]>;
def MOV16ri : I8086Inst<0xB8, AddRegFrm, Imm16, (outs GR16:$dst), (ins i16imm:$imm),
                        "mov\t$dst, $imm", [(set GR16:$dst, imm:$imm)]>;

// MOV r/m, imm  (opcode C6/C7 /0).  These take a *memory* operand only (mem8e/
// mem16e), so C6/C7 is never emitted with a register operand -- a register
// destination always uses the shorter B0-BF form above.  The store-immediate
// pattern folds `mov reg,imm; mov [mem],reg` into a single instruction.
def MOV8mi  : I8086Inst<0xC6, MRM0m, Imm8,  (outs), (ins mem8e:$dst, i8imm:$imm),
                        "mov\t$dst, $imm", [(store (i8 imm:$imm), addr:$dst)]>;
def MOV16mi : I8086Inst<0xC7, MRM0m, Imm16, (outs), (ins mem16e:$dst, i16imm:$imm),
                        "mov\t$dst, $imm", [(store (i16 imm:$imm), addr:$dst)]>;

// MOV involving segment registers (8C / 8E).
def MOV16rs : I8086Inst<0x8C, MRMDestReg, NoImm, (outs), (ins GR16:$dst, SEGR:$src), "mov\t$dst, $src">;
def MOV16ms : I8086Inst<0x8C, MRMDestMem, NoImm, (outs), (ins mem16:$dst, SEGR:$src),"mov\t$dst, $src">;
def MOVsr16 : I8086Inst<0x8E, MRMSrcReg,  NoImm, (outs), (ins SEGR:$dst, GR16:$src), "mov\t$dst, $src">;
def MOVsm16 : I8086Inst<0x8E, MRMSrcMem,  NoImm, (outs), (ins SEGR:$dst, mem16:$src),"mov\t$dst, $src">;

//===----------------------------------------------------------------------===//
// Data transfer: PUSH / POP / XCHG / IN / OUT / LEA / LDS / LES / XLAT.
//===----------------------------------------------------------------------===//

let Defs = [SP], Uses = [SP] in
def PUSH16r  : I8086Inst<0x50, AddRegFrm, NoImm, (outs), (ins GR16:$reg),
                         "push\t$reg", [(I8086push GR16:$reg)]>;
def PUSH16m  : I8086Inst<0xFF, MRM6m, NoImm, (outs), (ins mem16:$mem), "push\t$mem">;
def POP16r   : I8086Inst<0x58, AddRegFrm, NoImm, (outs GR16:$reg), (ins), "pop\t$reg">;
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

// Accumulator short form: `xchg ax, r16` is 0x90+r (1 byte) vs the 0x87 ModRM
// form (2 bytes).  AX is implicit in the opcode, so the *other* register is what
// gets encoded -- it must be operand 0 (AddRegFrm encodes operand 0), with AX a
// fixed AREG16 operand for matching (which makes the assembler prefer this form
// over XCHG16rr).  The asm string prints AX first ("xchg ax, r16"); the alias
// accepts the reversed operand order.  `xchg ax, ax` encodes 0x90 (NOP).
def XCHG16ar : I8086Inst<0x90, AddRegFrm, NoImm, (outs), (ins GR16:$reg, AREG16:$acc),
                         "xchg\t$acc, $reg">;
def : InstAlias<"xchg\t$reg, $acc", (XCHG16ar GR16:$reg, AREG16:$acc)>;

def IN8ri  : I8086Inst<0xE4, RawFrm, Imm8, (outs), (ins AREG8:$dst, i8imm:$port),  "in\t$dst, $port">;
def IN16ri : I8086Inst<0xE5, RawFrm, Imm8, (outs), (ins AREG16:$dst, i8imm:$port), "in\t$dst, $port">;
def IN8rr  : I8086Inst<0xEC, RawFrm, NoImm, (outs), (ins AREG8:$dst, DREG16:$dx),  "in\t$dst, $dx">;
def IN16rr : I8086Inst<0xED, RawFrm, NoImm, (outs), (ins AREG16:$dst, DREG16:$dx), "in\t$dst, $dx">;
def OUT8ri  : I8086Inst<0xE6, RawFrm, Imm8, (outs), (ins AREG8:$src, i8imm:$port),  "out\t$port, $src">;
def OUT16ri : I8086Inst<0xE7, RawFrm, Imm8, (outs), (ins AREG16:$src, i8imm:$port), "out\t$port, $src">;
def OUT8rr  : I8086Inst<0xEE, RawFrm, NoImm, (outs), (ins DREG16:$dx, AREG8:$src),  "out\t$dx, $src">;
def OUT16rr : I8086Inst<0xEF, RawFrm, NoImm, (outs), (ins DREG16:$dx, AREG16:$src), "out\t$dx, $src">;

def LEA16 : I8086Inst<0x8D, MRMSrcMem, NoImm, (outs), (ins GR16:$dst, anymem:$src), "lea\t$dst, $src">;
// CodeGen form (defines $dst, no memory access, no FLAGS): built by the post-RA
// I8086FormLea pass to collapse a mov/add chain that computes base+index+disp.
let isCodeGenOnly = 1 in
def LEA16r : I8086Inst<0x8D, MRMSrcMem, NoImm, (outs GR16:$dst), (ins anymem:$src), "lea\t$dst, $src">;
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

// Flag-setting binary ALU with CodeGen patterns.  Register/immediate forms are
// two-address (dst tied to src1) and set FLAGS.  Memory operands are folded: a
// single-use load reads directly into the op (reg,mem), and a load+op+store to
// one address becomes a read-modify-write (mem,reg / mem,imm).
multiclass BinOp<string m, bits<8> base, Format ri, Format mi, SDNode node,
                 bit commutable> {
  let Defs = [FLAGS], Constraints = "$src1 = $dst" in {
    let isCommutable = commutable in {
      def 8rr : I8086Inst<!add(base,0), MRMDestReg, NoImm,
                          (outs GR8:$dst), (ins GR8:$src1, GR8:$src2),
                          m#"\t$dst, $src2",
                          [(set GR8:$dst, (node GR8:$src1, GR8:$src2))]>;
      def 16rr : I8086Inst<!add(base,1), MRMDestReg, NoImm,
                           (outs GR16:$dst), (ins GR16:$src1, GR16:$src2),
                           m#"\t$dst, $src2",
                           [(set GR16:$dst, (node GR16:$src1, GR16:$src2))]>;
    }
    // reg,imm forms.  AddedComplexity keeps a constant operand as an immediate
    // rather than letting isel materialize it into a register to feed the
    // load-fold below; the post-RA I8086FoldMemImm pass then folds the memory
    // operand back in only where that is actually smaller.
    let AddedComplexity = 30 in {
    def 8ri : I8086Inst<0x80, ri, Imm8,
                        (outs GR8:$dst), (ins GR8:$src1, i8imm:$imm),
                        m#"\t$dst, $imm",
                        [(set GR8:$dst, (node GR8:$src1, imm:$imm))]>;
    def 16ri : I8086Inst<0x81, ri, Imm16,
                         (outs GR16:$dst), (ins GR16:$src1, i16imm:$imm),
                         m#"\t$dst, $imm",
                         [(set GR16:$dst, (node GR16:$src1, imm:$imm))]>;
    // Compact sign-extended-imm8 form (opcode 0x83); preferred when the
    // immediate fits in a byte.
    def 16ri8 : I8086Inst<0x83, ri, Imm8,
                          (outs GR16:$dst), (ins GR16:$src1, i16i8imm:$imm),
                          m#"\t$dst, $imm",
                          [(set GR16:$dst, (node GR16:$src1, i16immSExt8:$imm))]>;
    } // AddedComplexity
    // Load-folded: reg = reg OP mem.  Selected for a real value operand; a
    // constant stays an immediate (the reg,imm forms above outrank this), and
    // the post-RA fold decides whether to pull the memory operand in.
    let mayLoad = 1 in {
      def 8rm : I8086Inst<!add(base,2), MRMSrcMem, NoImm,
                          (outs GR8:$dst), (ins GR8:$src1, mem8:$src2),
                          m#"\t$dst, $src2",
                          [(set GR8:$dst, (node GR8:$src1, (load addr:$src2)))]>;
      def 16rm : I8086Inst<!add(base,3), MRMSrcMem, NoImm,
                           (outs GR16:$dst), (ins GR16:$src1, mem16:$src2),
                           m#"\t$dst, $src2",
                           [(set GR16:$dst, (node GR16:$src1, (load addr:$src2)))]>;
    }
  }
  // Read-modify-write memory forms: mem = mem OP reg / imm.
  let Defs = [FLAGS], mayLoad = 1, mayStore = 1 in {
    def 8mr : I8086Inst<!add(base,0), MRMDestMem, NoImm, (outs), (ins mem8:$dst, GR8:$src),
                        m#"\t$dst, $src",
                        [(store (node (load addr:$dst), GR8:$src), addr:$dst)]>;
    def 16mr : I8086Inst<!add(base,1), MRMDestMem, NoImm, (outs), (ins mem16:$dst, GR16:$src),
                         m#"\t$dst, $src",
                         [(store (node (load addr:$dst), GR16:$src), addr:$dst)]>;
    def 8mi : I8086Inst<0x80, mi, Imm8, (outs), (ins mem8e:$dst, i8imm:$imm),
                        m#"\t$dst, $imm",
                        [(store (node (load addr:$dst), (i8 imm:$imm)), addr:$dst)]>;
    def 16mi : I8086Inst<0x81, mi, Imm16, (outs), (ins mem16e:$dst, i16imm:$imm),
                         m#"\t$dst, $imm",
                         [(store (node (load addr:$dst), (i16 imm:$imm)), addr:$dst)]>;
    def 16mi8 : I8086Inst<0x83, mi, Imm8, (outs), (ins mem16e:$dst, i16i8imm:$imm),
                          m#"\t$dst, $imm",
                          [(store (node (load addr:$dst), i16immSExt8:$imm), addr:$dst)]>;
  }
}

// Carry-consuming ALU (ADC/SBB) for the high words of wide add/sub.  Identical
// to BinOp but also reads FLAGS (the carry in), matching the glue-based adde/
// sube nodes the type legalizer produces when expanding i32/i64 add/sub.
multiclass BinOpC<string m, bits<8> base, Format ri, Format mi, SDNode node,
                  bit commutable> {
  let Defs = [FLAGS], Uses = [FLAGS], Constraints = "$src1 = $dst" in {
    let isCommutable = commutable in {
      def 8rr : I8086Inst<!add(base,0), MRMDestReg, NoImm,
                          (outs GR8:$dst), (ins GR8:$src1, GR8:$src2),
                          m#"\t$dst, $src2",
                          [(set GR8:$dst, (node GR8:$src1, GR8:$src2))]>;
      def 16rr : I8086Inst<!add(base,1), MRMDestReg, NoImm,
                           (outs GR16:$dst), (ins GR16:$src1, GR16:$src2),
                           m#"\t$dst, $src2",
                           [(set GR16:$dst, (node GR16:$src1, GR16:$src2))]>;
    }
    def 8ri : I8086Inst<0x80, ri, Imm8,
                        (outs GR8:$dst), (ins GR8:$src1, i8imm:$imm),
                        m#"\t$dst, $imm",
                        [(set GR8:$dst, (node GR8:$src1, imm:$imm))]>;
    def 16ri : I8086Inst<0x81, ri, Imm16,
                         (outs GR16:$dst), (ins GR16:$src1, i16imm:$imm),
                         m#"\t$dst, $imm",
                         [(set GR16:$dst, (node GR16:$src1, imm:$imm))]>;
    def 16ri8 : I8086Inst<0x83, ri, Imm8,
                          (outs GR16:$dst), (ins GR16:$src1, i16i8imm:$imm),
                          m#"\t$dst, $imm",
                          [(set GR16:$dst, (node GR16:$src1, i16immSExt8:$imm))]>;
  }
  // Assembler-only memory forms.
  def 8mr : I8086Inst<!add(base,0), MRMDestMem, NoImm, (outs), (ins mem8:$dst, GR8:$src),   m#"\t$dst, $src">;
  def 16mr : I8086Inst<!add(base,1), MRMDestMem, NoImm, (outs), (ins mem16:$dst, GR16:$src), m#"\t$dst, $src">;
  def 8rm : I8086Inst<!add(base,2), MRMSrcMem, NoImm, (outs), (ins GR8:$dst, mem8:$src),   m#"\t$dst, $src">;
  def 16rm : I8086Inst<!add(base,3), MRMSrcMem, NoImm, (outs), (ins GR16:$dst, mem16:$src), m#"\t$dst, $src">;
  def 8mi : I8086Inst<0x80, mi, Imm8, (outs), (ins mem8e:$dst, i8imm:$imm),   m#"\t$dst, $imm">;
  def 16mi : I8086Inst<0x81, mi, Imm16, (outs), (ins mem16e:$dst, i16imm:$imm), m#"\t$dst, $imm">;
  def 16mi8 : I8086Inst<0x83, mi, Imm8, (outs), (ins mem16e:$dst, i16i8imm:$imm), m#"\t$dst, $imm">;
}

defm ADD : BinOp<"add", 0x00, MRM0r, MRM0m, add, 1>;
defm OR  : BinOp<"or",  0x08, MRM1r, MRM1m, or,  1>;
defm AND : BinOp<"and", 0x20, MRM4r, MRM4m, and, 1>;
defm SUB : BinOp<"sub", 0x28, MRM5r, MRM5m, sub, 0>;
defm XOR : BinOp<"xor", 0x30, MRM6r, MRM6m, xor, 1>;

defm ADC : BinOpC<"adc", 0x10, MRM2r, MRM2m, adde, 1>;
defm SBB : BinOpC<"sbb", 0x18, MRM3r, MRM3m, sube, 0>;

// Wide (i32/i64) add/sub are expanded by the type legalizer into a carry chain:
// the low word is a plain ADD/SUB that *produces* the carry (addc/subc), and the
// high word(s) are ADC/SBB that consume it (adde/sube, handled by the patterns
// in BinOpC above).  Map the carry-producing low word onto ADD/SUB.
def : Pat<(addc GR16:$a, GR16:$b),         (ADD16rr  GR16:$a, GR16:$b)>;
def : Pat<(addc GR16:$a, i16immSExt8:$b),  (ADD16ri8 GR16:$a, i16immSExt8:$b)>;
def : Pat<(addc GR16:$a, imm:$b),          (ADD16ri  GR16:$a, imm:$b)>;
def : Pat<(addc GR8:$a,  GR8:$b),          (ADD8rr   GR8:$a,  GR8:$b)>;
def : Pat<(addc GR8:$a,  imm:$b),          (ADD8ri   GR8:$a,  imm:$b)>;
def : Pat<(subc GR16:$a, GR16:$b),         (SUB16rr  GR16:$a, GR16:$b)>;
def : Pat<(subc GR16:$a, i16immSExt8:$b),  (SUB16ri8 GR16:$a, i16immSExt8:$b)>;
def : Pat<(subc GR16:$a, imm:$b),          (SUB16ri  GR16:$a, imm:$b)>;
def : Pat<(subc GR8:$a,  GR8:$b),          (SUB8rr   GR8:$a,  GR8:$b)>;
def : Pat<(subc GR8:$a,  imm:$b),          (SUB8ri   GR8:$a,  imm:$b)>;

// CMP sets FLAGS without writing a destination.
let Defs = [FLAGS] in {
  def CMP8rr  : I8086Inst<0x38, MRMDestReg, NoImm, (outs), (ins GR8:$src1, GR8:$src2),
                          "cmp\t$src1, $src2", [(I8086cmp GR8:$src1, GR8:$src2)]>;
  def CMP16rr : I8086Inst<0x39, MRMDestReg, NoImm, (outs), (ins GR16:$src1, GR16:$src2),
                          "cmp\t$src1, $src2", [(I8086cmp GR16:$src1, GR16:$src2)]>;
  def CMP8ri  : I8086Inst<0x80, MRM7r, Imm8, (outs), (ins GR8:$src1, i8imm:$imm),
                          "cmp\t$src1, $imm", [(I8086cmp GR8:$src1, imm:$imm)]>;
  def CMP16ri : I8086Inst<0x81, MRM7r, Imm16, (outs), (ins GR16:$src1, i16imm:$imm),
                          "cmp\t$src1, $imm", [(I8086cmp GR16:$src1, imm:$imm)]>;
  def CMP16ri8 : I8086Inst<0x83, MRM7r, Imm8, (outs), (ins GR16:$src1, i16i8imm:$imm),
                           "cmp\t$src1, $imm", [(I8086cmp GR16:$src1, i16immSExt8:$imm)]>;
  // Memory-folded: compare a single-use load directly (either operand order).
  let mayLoad = 1 in {
  def CMP8mr  : I8086Inst<0x38, MRMDestMem, NoImm, (outs), (ins mem8:$dst, GR8:$src),
                          "cmp\t$dst, $src", [(I8086cmp (load addr:$dst), GR8:$src)]>;
  def CMP16mr : I8086Inst<0x39, MRMDestMem, NoImm, (outs), (ins mem16:$dst, GR16:$src),
                          "cmp\t$dst, $src", [(I8086cmp (load addr:$dst), GR16:$src)]>;
  def CMP8rm  : I8086Inst<0x3A, MRMSrcMem, NoImm, (outs), (ins GR8:$dst, mem8:$src),
                          "cmp\t$dst, $src", [(I8086cmp GR8:$dst, (load addr:$src))]>;
  def CMP16rm : I8086Inst<0x3B, MRMSrcMem, NoImm, (outs), (ins GR16:$dst, mem16:$src),
                          "cmp\t$dst, $src", [(I8086cmp GR16:$dst, (load addr:$src))]>;
  def CMP8mi  : I8086Inst<0x80, MRM7m, Imm8, (outs), (ins mem8e:$dst, i8imm:$imm),
                          "cmp\t$dst, $imm", [(I8086cmp (load addr:$dst), (i8 imm:$imm))]>;
  def CMP16mi : I8086Inst<0x81, MRM7m, Imm16, (outs), (ins mem16e:$dst, i16imm:$imm),
                          "cmp\t$dst, $imm", [(I8086cmp (load addr:$dst), (i16 imm:$imm))]>;
  def CMP16mi8 : I8086Inst<0x83, MRM7m, Imm8, (outs), (ins mem16e:$dst, i16i8imm:$imm),
                           "cmp\t$dst, $imm", [(I8086cmp (load addr:$dst), i16immSExt8:$imm)]>;
  }
}

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
// Accumulator (AL/AX) short forms.  AL/AX is implicit in the opcode (RawFrm),
// so the register operand is not encoded: `<op> al,imm8` is 2 bytes and
// `<op> ax,imm16` 3 bytes -- one shorter than the ModRM r/m,imm forms.  The
// AREG8/AREG16 single-register operand classes are subclasses of GR8/GR16, so
// the assembler prefers these when the destination is AL/AX.
//===----------------------------------------------------------------------===//

multiclass AccArith<string m, bits<8> op, list<Register> uses = []> {
  let Defs = [FLAGS], Uses = uses, Constraints = "$src1 = $dst" in {
    def 8i  : I8086Inst<op,         RawFrm, Imm8,  (outs AREG8:$dst),
                        (ins AREG8:$src1, i8imm:$imm),  m#"\t$dst, $imm">;
    def 16i : I8086Inst<!add(op,1), RawFrm, Imm16, (outs AREG16:$dst),
                        (ins AREG16:$src1, i16imm:$imm), m#"\t$dst, $imm">;
  }
}
defm ADD : AccArith<"add", 0x04>;
defm OR  : AccArith<"or",  0x0C>;
defm ADC : AccArith<"adc", 0x14, [FLAGS]>;
defm SBB : AccArith<"sbb", 0x1C, [FLAGS]>;
defm AND : AccArith<"and", 0x24>;
defm SUB : AccArith<"sub", 0x2C>;
defm XOR : AccArith<"xor", 0x34>;

let Defs = [FLAGS] in {
  def CMP8i   : I8086Inst<0x3C, RawFrm, Imm8,  (outs), (ins AREG8:$src1,  i8imm:$imm),  "cmp\t$src1, $imm">;
  def CMP16i  : I8086Inst<0x3D, RawFrm, Imm16, (outs), (ins AREG16:$src1, i16imm:$imm), "cmp\t$src1, $imm">;
  def TEST8i  : I8086Inst<0xA8, RawFrm, Imm8,  (outs), (ins AREG8:$src1,  i8imm:$imm),  "test\t$src1, $imm">;
  def TEST16i : I8086Inst<0xA9, RawFrm, Imm16, (outs), (ins AREG16:$src1, i16imm:$imm), "test\t$src1, $imm">;
}

//===----------------------------------------------------------------------===//
// INC / DEC / NOT / NEG / MUL / IMUL / DIV / IDIV.
//===----------------------------------------------------------------------===//

// INC/DEC: two-address CodeGen forms selecting for +1 / -1 (1 byte for r16,
// 2 for r8, vs 3 for the 0x83 imm8 form).  Unlike ADD/SUB these leave CF
// *unchanged*, so they must never stand in for the carry-producing low word of
// a wide add/sub -- but that word is an addc/subc node (mapped to ADD/SUB) while
// these match plain add, so there is no overlap.  They still clobber the other
// arithmetic flags (ZF/SF/OF/AF/PF), hence Defs=[FLAGS].
let AddedComplexity = 30, Constraints = "$src1 = $dst", Defs = [FLAGS] in {
  def INC16r : I8086Inst<0x40, AddRegFrm, NoImm, (outs GR16:$dst), (ins GR16:$src1),
                         "inc\t$dst", [(set GR16:$dst, (add GR16:$src1, 1))]>;
  def DEC16r : I8086Inst<0x48, AddRegFrm, NoImm, (outs GR16:$dst), (ins GR16:$src1),
                         "dec\t$dst", [(set GR16:$dst, (add GR16:$src1, -1))]>;
  def INC8r  : I8086Inst<0xFE, MRM0r, NoImm, (outs GR8:$dst), (ins GR8:$src1),
                         "inc\t$dst", [(set GR8:$dst, (add GR8:$src1, 1))]>;
  def DEC8r  : I8086Inst<0xFE, MRM1r, NoImm, (outs GR8:$dst), (ins GR8:$src1),
                         "dec\t$dst", [(set GR8:$dst, (add GR8:$src1, -1))]>;
}
def INC8m  : I8086Inst<0xFE, MRM0m, NoImm, (outs), (ins mem8e:$mem), "inc\t$mem">;
def DEC8m  : I8086Inst<0xFE, MRM1m, NoImm, (outs), (ins mem8e:$mem), "dec\t$mem">;
def INC16m : I8086Inst<0xFF, MRM0m, NoImm, (outs), (ins mem16e:$mem), "inc\t$mem">;
def DEC16m : I8086Inst<0xFF, MRM1m, NoImm, (outs), (ins mem16e:$mem), "dec\t$mem">;

// Group 3 (F6/F7): NOT=/2 NEG=/3 MUL=/4 IMUL=/5 DIV=/6 IDIV=/7.  MUL/IMUL/DIV/
// IDIV are asm-only here (CodeGen uses the AX/DX-modeled MUL16i/DIV16i below).
multiclass Grp3<string m, Format r8, Format m8, Format r16, Format m16> {
  def 8r  : I8086Inst<0xF6, r8,  NoImm, (outs), (ins GR8:$reg),   m#"\t$reg">;
  def 8m  : I8086Inst<0xF6, m8,  NoImm, (outs), (ins mem8e:$mem),  m#"\t$mem">;
  def 16r : I8086Inst<0xF7, r16, NoImm, (outs), (ins GR16:$reg),  m#"\t$reg">;
  def 16m : I8086Inst<0xF7, m16, NoImm, (outs), (ins mem16e:$mem), m#"\t$mem">;
}
defm MUL  : Grp3<"mul",  MRM4r, MRM4m, MRM4r, MRM4m>;
defm IMUL : Grp3<"imul", MRM5r, MRM5m, MRM5r, MRM5m>;
defm DIV  : Grp3<"div",  MRM6r, MRM6m, MRM6r, MRM6m>;
defm IDIV : Grp3<"idiv", MRM7r, MRM7m, MRM7r, MRM7m>;

// NOT (~x): two-address register CodeGen form -- 2 bytes / 3 cycles and, unlike
// `xor r,-1` (3-4 bytes, 4 cycles), leaves FLAGS untouched.  Asm-only mem forms.
let AddedComplexity = 30, Constraints = "$src1 = $dst" in {
  def NOT8r  : I8086Inst<0xF6, MRM2r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),
                         "not\t$dst", [(set GR8:$dst,  (not GR8:$src1))]>;
  def NOT16r : I8086Inst<0xF7, MRM2r, NoImm, (outs GR16:$dst), (ins GR16:$src1),
                         "not\t$dst", [(set GR16:$dst, (not GR16:$src1))]>;
}
def NOT8m  : I8086Inst<0xF6, MRM2m, NoImm, (outs), (ins mem8e:$mem),  "not\t$mem">;
def NOT16m : I8086Inst<0xF7, MRM2m, NoImm, (outs), (ins mem16e:$mem), "not\t$mem">;

// NEG (-x = 0-x): two-address register CodeGen form -- one in-place instruction
// (2 bytes) that sets FLAGS, versus the `xor r,r; sub r,x` pair otherwise.
let AddedComplexity = 30, Constraints = "$src1 = $dst", Defs = [FLAGS] in {
  def NEG8r  : I8086Inst<0xF6, MRM3r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),
                         "neg\t$dst", [(set GR8:$dst,  (ineg GR8:$src1))]>;
  def NEG16r : I8086Inst<0xF7, MRM3r, NoImm, (outs GR16:$dst), (ins GR16:$src1),
                         "neg\t$dst", [(set GR16:$dst, (ineg GR16:$src1))]>;
}
def NEG8m  : I8086Inst<0xF6, MRM3m, NoImm, (outs), (ins mem8e:$mem),  "neg\t$mem">;
def NEG16m : I8086Inst<0xF7, MRM3m, NoImm, (outs), (ins mem16e:$mem), "neg\t$mem">;

// Native 16-bit multiply/divide with the implicit AX/DX operands modeled, for
// CodeGen (the assembler uses the plain MUL16r/etc above).  MUL: DX:AX = AX *
// src.  DIV: AX = DX:AX / src, DX = DX:AX % src.
let isCodeGenOnly = 1, Defs = [AX, DX, FLAGS], Uses = [AX] in {
  def MUL16i  : I8086Inst<0xF7, MRM4r, NoImm, (outs), (ins GR16:$src), "mul\t$src">;
  def IMUL16i : I8086Inst<0xF7, MRM5r, NoImm, (outs), (ins GR16:$src), "imul\t$src">;
}
let isCodeGenOnly = 1, Defs = [AX, DX, FLAGS], Uses = [AX, DX] in {
  def DIV16i  : I8086Inst<0xF7, MRM6r, NoImm, (outs), (ins GR16:$src), "div\t$src">;
  def IDIV16i : I8086Inst<0xF7, MRM7r, NoImm, (outs), (ins GR16:$src), "idiv\t$src">;
}

// Pseudos expanded by EmitInstrWithCustomInserter into the above (copy operand
// to AX, set up DX, run the instruction, copy the result(s) out).
let isCodeGenOnly = 1, usesCustomInserter = 1, Defs = [AX, DX, FLAGS] in {
  def UMULLOHI16 : I8086Pseudo<(outs GR16:$lo, GR16:$hi), (ins GR16:$a, GR16:$b),
                               "#UMULLOHI16",
                               [(set GR16:$lo, GR16:$hi, (umullohi GR16:$a, GR16:$b))]>;
  def SMULLOHI16 : I8086Pseudo<(outs GR16:$lo, GR16:$hi), (ins GR16:$a, GR16:$b),
                               "#SMULLOHI16",
                               [(set GR16:$lo, GR16:$hi, (smullohi GR16:$a, GR16:$b))]>;
  def UDIVREM16  : I8086Pseudo<(outs GR16:$q, GR16:$r), (ins GR16:$a, GR16:$b),
                               "#UDIVREM16",
                               [(set GR16:$q, GR16:$r, (udivrem GR16:$a, GR16:$b))]>;
  def SDIVREM16  : I8086Pseudo<(outs GR16:$q, GR16:$r), (ins GR16:$a, GR16:$b),
                               "#SDIVREM16",
                               [(set GR16:$q, GR16:$r, (sdivrem GR16:$a, GR16:$b))]>;
}

// Decimal / ASCII adjust and sign extension.
def DAA : NullaryInst<0x27, "daa">;
def DAS : NullaryInst<0x2F, "das">;
def AAA : NullaryInst<0x37, "aaa">;
def AAS : NullaryInst<0x3F, "aas">;
def AAM : I8086Inst<0xD4, RawFrm, Imm8, (outs), (ins i8imm:$base), "aam\t$base">;
def AAD : I8086Inst<0xD5, RawFrm, Imm8, (outs), (ins i8imm:$base), "aad\t$base">;
let Defs = [AX], Uses = [AL] in
def CBW : NullaryInst<0x98, "cbw">;   // sign-extend AL -> AX
let Defs = [DX], Uses = [AX] in
def CWD : NullaryInst<0x99, "cwd">;   // sign-extend AX -> DX:AX

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

// Unconditional jumps.  The compiler selects the short rel8 JMP8 for `br`;
// branch relaxation grows it to the near rel16 JMP16 when the target is out of
// rel8 range.  JMP8 is CodeGenOnly so the assembler keeps emitting the near
// form for a plain `jmp label` (JMP16), avoiding a matcher ambiguity.
let isBranch = 1, isTerminator = 1, isBarrier = 1 in {
  def JMP16    : I8086Inst<0xE9, RawFrm, Rel16, (outs), (ins jmptarget:$dst),
                           "jmp\t$dst">;
  let isCodeGenOnly = 1 in
  def JMP8     : I8086Inst<0xEB, RawFrm, Rel8,  (outs), (ins jmptarget:$dst),
                           "jmp\t$dst", [(br bb:$dst)]>;
}
let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1 in {
  def JMP16r   : I8086Inst<0xFF, MRM4r, NoImm, (outs), (ins GR16:$dst),
                           "jmp\t$dst", [(brind GR16:$dst)]>;
  def JMP16m   : I8086Inst<0xFF, MRM4m, NoImm, (outs), (ins mem16:$dst),
                           "jmp\t$dst", [(brind (load addr:$dst))]>;
}
let isCodeGenOnly = 1 in
def JMPFAR   : I8086Inst<0xEA, RawFrmFar, NoImm, (outs), (ins i16imm:$off, i16imm:$seg), "jmp\t$seg:$off">;

let isCall = 1, Defs = [AX, BX, CX, DX, FLAGS], Uses = [SP] in {
  def CALL16   : I8086Inst<0xE8, RawFrm, Rel16, (outs), (ins brtarget16:$dst),
                           "call\t$dst", [(I8086call imm:$dst)]>;
  def CALL16r  : I8086Inst<0xFF, MRM2r, NoImm, (outs), (ins GR16:$dst),
                           "call\t$dst", [(I8086call GR16:$dst)]>;
  def CALL16m  : I8086Inst<0xFF, MRM2m, NoImm, (outs), (ins mem16:$dst),
                           "call\t$dst", [(I8086call (load addr:$dst))]>;
}
let isCodeGenOnly = 1 in
def CALLFAR  : I8086Inst<0x9A, RawFrmFar, NoImm, (outs), (ins i16imm:$off, i16imm:$seg), "call\t$seg:$off">;

let isReturn = 1, isTerminator = 1, isBarrier = 1 in {
  def RET      : I8086Inst<0xC3, RawFrm, NoImm, (outs), (ins), "ret", [(I8086retglue)]>;
  def RETI     : I8086Inst<0xC2, RawFrm, Imm16, (outs), (ins i16imm:$amt), "ret\t$amt">;
  def RETF     : NullaryInst<0xCB, "retf">;
  def RETFI    : I8086Inst<0xCA, RawFrm, Imm16, (outs), (ins i16imm:$amt), "retf\t$amt">;
}

// Conditional jumps (all rel8 on the 8086; read FLAGS).
let isBranch = 1, isTerminator = 1, Uses = [FLAGS] in
class Jcc<bits<8> op, string m>
    : I8086Inst<op, RawFrm, Rel8, (outs), (ins jmptarget:$dst), m#"\t$dst">;
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

// Map I8086ISD::BR_CC (target block + condition code) to the right Jcc.
// Condition codes follow I8086CC::CondCode (x86 Jcc nibble encoding).
def : Pat<(I8086brcc bb:$dst, (i8  0)), (JO  jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8  1)), (JNO jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8  2)), (JB  jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8  3)), (JAE jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8  4)), (JE  jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8  5)), (JNE jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8  6)), (JBE jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8  7)), (JA  jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8  8)), (JS  jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8  9)), (JNS jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8 10)), (JP  jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8 11)), (JNP jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8 12)), (JL  jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8 13)), (JGE jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8 14)), (JLE jmptarget:$dst)>;
def : Pat<(I8086brcc bb:$dst, (i8 15)), (JG  jmptarget:$dst)>;

// Direct calls to symbols.
def : Pat<(I8086call (i16 tglobaladdr:$dst)),  (CALL16 tglobaladdr:$dst)>;
def : Pat<(I8086call (i16 texternalsym:$dst)), (CALL16 texternalsym:$dst)>;

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

//===----------------------------------------------------------------------===//
// CodeGen-only pseudo instructions.
//===----------------------------------------------------------------------===//

// Call-frame setup/teardown.  They may become sub/add of SP (which sets FLAGS),
// so mark SP and FLAGS.
let isCodeGenOnly = 1, Defs = [SP, FLAGS], Uses = [SP] in {
  def ADJCALLSTACKDOWN : I8086Pseudo<(outs), (ins i16imm:$amt1, i16imm:$amt2),
                                     "#ADJCALLSTACKDOWN $amt1 $amt2",
                                     [(I8086callseq_start timm:$amt1, timm:$amt2)]>;
  def ADJCALLSTACKUP   : I8086Pseudo<(outs), (ins i16imm:$amt1, i16imm:$amt2),
                                     "#ADJCALLSTACKUP $amt1 $amt2",
                                     [(I8086callseq_end timm:$amt1, timm:$amt2)]>;
}

// Address of a stack slot; lowered by eliminateFrameIndex to
// mov + (optional) add, which clobbers FLAGS.
let isCodeGenOnly = 1, Defs = [FLAGS] in
def ADDframe : I8086Pseudo<(outs GR16:$dst), (ins i16imm:$base, i16imm:$offset),
                           "#ADDframe", []>;

// select: expanded to a control-flow diamond by EmitInstrWithCustomInserter
// (there is no SETcc/CMOV on the 8086).
let isCodeGenOnly = 1, usesCustomInserter = 1, Uses = [FLAGS] in {
  def Select8  : I8086Pseudo<(outs GR8:$dst), (ins GR8:$src1, GR8:$src2, i8imm:$cc),
                             "#Select8",
                             [(set GR8:$dst, (I8086selectcc GR8:$src1, GR8:$src2, imm:$cc))]>;
  def Select16 : I8086Pseudo<(outs GR16:$dst), (ins GR16:$src1, GR16:$src2, i8imm:$cc),
                             "#Select16",
                             [(set GR16:$dst, (I8086selectcc GR16:$src1, GR16:$src2, imm:$cc))]>;
}

// Shift-by-CL codegen instructions (D2/D3 /digit).  Two-address; CL is an
// implicit count.  Built by the custom inserter below.
let isCodeGenOnly = 1, Constraints = "$src1 = $dst", Defs = [FLAGS],
    Uses = [CL] in {
  def SHL8CL  : I8086Inst<0xD2, MRM4r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),  "shl\t$dst, cl">;
  def SHR8CL  : I8086Inst<0xD2, MRM5r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),  "shr\t$dst, cl">;
  def SAR8CL  : I8086Inst<0xD2, MRM7r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),  "sar\t$dst, cl">;
  def SHL16CL : I8086Inst<0xD3, MRM4r, NoImm, (outs GR16:$dst), (ins GR16:$src1), "shl\t$dst, cl">;
  def SHR16CL : I8086Inst<0xD3, MRM5r, NoImm, (outs GR16:$dst), (ins GR16:$src1), "shr\t$dst, cl">;
  def SAR16CL : I8086Inst<0xD3, MRM7r, NoImm, (outs GR16:$dst), (ins GR16:$src1), "sar\t$dst, cl">;
  def ROL8CL  : I8086Inst<0xD2, MRM0r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),  "rol\t$dst, cl">;
  def ROR8CL  : I8086Inst<0xD2, MRM1r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),  "ror\t$dst, cl">;
  def ROL16CL : I8086Inst<0xD3, MRM0r, NoImm, (outs GR16:$dst), (ins GR16:$src1), "rol\t$dst, cl">;
  def ROR16CL : I8086Inst<0xD3, MRM1r, NoImm, (outs GR16:$dst), (ins GR16:$src1), "ror\t$dst, cl">;
}

// Shift/rotate-by-1 codegen instructions (D0/D1 /digit, 2 bytes, 2 cycles).
// Two-address; used for constant counts of 1 (once) and 2 (twice) -- both faster
// than the CL form and no larger.
let isCodeGenOnly = 1, Constraints = "$src1 = $dst", Defs = [FLAGS] in {
  def SHL8b1  : I8086Inst<0xD0, MRM4r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),  "shl\t$dst, 1">;
  def SHL16b1 : I8086Inst<0xD1, MRM4r, NoImm, (outs GR16:$dst), (ins GR16:$src1), "shl\t$dst, 1">;
  def SHR8b1  : I8086Inst<0xD0, MRM5r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),  "shr\t$dst, 1">;
  def SHR16b1 : I8086Inst<0xD1, MRM5r, NoImm, (outs GR16:$dst), (ins GR16:$src1), "shr\t$dst, 1">;
  def SAR8b1  : I8086Inst<0xD0, MRM7r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),  "sar\t$dst, 1">;
  def SAR16b1 : I8086Inst<0xD1, MRM7r, NoImm, (outs GR16:$dst), (ins GR16:$src1), "sar\t$dst, 1">;
  def ROL8b1  : I8086Inst<0xD0, MRM0r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),  "rol\t$dst, 1">;
  def ROL16b1 : I8086Inst<0xD1, MRM0r, NoImm, (outs GR16:$dst), (ins GR16:$src1), "rol\t$dst, 1">;
  def ROR8b1  : I8086Inst<0xD0, MRM1r, NoImm, (outs GR8:$dst),  (ins GR8:$src1),  "ror\t$dst, 1">;
  def ROR16b1 : I8086Inst<0xD1, MRM1r, NoImm, (outs GR16:$dst), (ins GR16:$src1), "ror\t$dst, 1">;
}

// Shift pseudos: EmitInstrWithCustomInserter copies the count into CL and emits
// the shift-by-CL instruction above.  The expansion clobbers CL, so it must be
// declared here: the custom inserter runs during ISel, and the isel scheduler
// relies on this Defs list to serialize any other live CL/CX value correctly.
let isCodeGenOnly = 1, usesCustomInserter = 1, Defs = [CL, FLAGS] in {
  def Shl8  : I8086Pseudo<(outs GR8:$dst),  (ins GR8:$src, GR8:$amt),  "#Shl8",  [(set GR8:$dst,  (shl GR8:$src, GR8:$amt))]>;
  def Shr8  : I8086Pseudo<(outs GR8:$dst),  (ins GR8:$src, GR8:$amt),  "#Shr8",  [(set GR8:$dst,  (srl GR8:$src, GR8:$amt))]>;
  def Sar8  : I8086Pseudo<(outs GR8:$dst),  (ins GR8:$src, GR8:$amt),  "#Sar8",  [(set GR8:$dst,  (sra GR8:$src, GR8:$amt))]>;
  def Shl16 : I8086Pseudo<(outs GR16:$dst), (ins GR16:$src, GR8:$amt), "#Shl16", [(set GR16:$dst, (shl GR16:$src, GR8:$amt))]>;
  def Shr16 : I8086Pseudo<(outs GR16:$dst), (ins GR16:$src, GR8:$amt), "#Shr16", [(set GR16:$dst, (srl GR16:$src, GR8:$amt))]>;
  def Sar16 : I8086Pseudo<(outs GR16:$dst), (ins GR16:$src, GR8:$amt), "#Sar16", [(set GR16:$dst, (sra GR16:$src, GR8:$amt))]>;
  def Rol8  : I8086Pseudo<(outs GR8:$dst),  (ins GR8:$src, GR8:$amt),  "#Rol8",  [(set GR8:$dst,  (rotl GR8:$src, GR8:$amt))]>;
  def Ror8  : I8086Pseudo<(outs GR8:$dst),  (ins GR8:$src, GR8:$amt),  "#Ror8",  [(set GR8:$dst,  (rotr GR8:$src, GR8:$amt))]>;
  def Rol16 : I8086Pseudo<(outs GR16:$dst), (ins GR16:$src, GR8:$amt), "#Rol16", [(set GR16:$dst, (rotl GR16:$src, GR8:$amt))]>;
  def Ror16 : I8086Pseudo<(outs GR16:$dst), (ins GR16:$src, GR8:$amt), "#Ror16", [(set GR16:$dst, (rotr GR16:$src, GR8:$amt))]>;
}

// Constant shift/rotate by 1 or 2 uses the D0/D1 by-1 form (2 cycles each), which
// is faster than -- and no larger than -- the CL form.  These patterns are more
// specific than the GR8:$amt pseudo above, so they win for counts 1 and 2; counts
// >= 3 fall through to the (smaller) CL pseudo.
multiclass By1Pat<SDNode node, Instruction Op8, Instruction Op16> {
  def : Pat<(node GR8:$s,  (i8 1)), (Op8 GR8:$s)>;
  def : Pat<(node GR8:$s,  (i8 2)), (Op8 (Op8 GR8:$s))>;
  def : Pat<(node GR16:$s, (i8 1)), (Op16 GR16:$s)>;
  def : Pat<(node GR16:$s, (i8 2)), (Op16 (Op16 GR16:$s))>;
}
defm : By1Pat<shl,  SHL8b1, SHL16b1>;
defm : By1Pat<srl,  SHR8b1, SHR16b1>;
defm : By1Pat<sra,  SAR8b1, SAR16b1>;
defm : By1Pat<rotl, ROL8b1, ROL16b1>;
defm : By1Pat<rotr, ROR8b1, ROR16b1>;

//===----------------------------------------------------------------------===//
// GlobalAddress / ExternalSymbol wrappers (materialize a 16-bit address).
//===----------------------------------------------------------------------===//

def : Pat<(i16 (I8086Wrapper tglobaladdr:$dst)),  (MOV16ri tglobaladdr:$dst)>;
def : Pat<(i16 (I8086Wrapper texternalsym:$dst)), (MOV16ri texternalsym:$dst)>;
def : Pat<(i16 (I8086Wrapper tblockaddress:$dst)),(MOV16ri tblockaddress:$dst)>;
def : Pat<(i16 (I8086Wrapper tjumptable:$dst)),   (MOV16ri tjumptable:$dst)>;

// Materialize a zero via XOR (2 bytes) rather than MOV reg,0 (3 bytes).  These
// clobber FLAGS, so they are only selected where the flags are dead; the
// scheduler serializes them out of any live-flags region.  Expanded to a real
// XOR reg,reg post-RA (see I8086InstrInfo::expandPostRAPseudo).
let isReMaterializable = 1, isAsCheapAsAMove = 1, isMoveImm = 1,
    hasSideEffects = 0, Defs = [FLAGS] in {
  def MOV8r0  : I8086Pseudo<(outs GR8:$dst),  (ins), "#MOV8r0",  [(set GR8:$dst,  (i8 0))]>;
  def MOV16r0 : I8086Pseudo<(outs GR16:$dst), (ins), "#MOV16r0", [(set GR16:$dst, (i16 0))]>;
}

// i8 <-> i16 conversions.  The 8086 has no MOVZX/MOVSX and 8-bit ops leave the
// high byte untouched, so:
//   trunc  -> take the low 8-bit subregister
//   anyext -> place the byte in the low half of an undef word
//   zext   -> put the byte in the low half of a word whose high half is zeroed;
//             this becomes `xor <hi>,<hi>` (2 bytes) rather than `and reg,0xff`,
//             and constrains the result to AX/CX/DX/BX (the regs with a high
//             byte).  If the source coalesces into the low byte, only the
//             high-byte clear remains.
def : Pat<(i8  (trunc GR16:$src)), (EXTRACT_SUBREG GR16:$src, sub_8bit_lo)>;
def : Pat<(i16 (anyext GR8:$src)),
          (INSERT_SUBREG (i16 (IMPLICIT_DEF)), GR8:$src, sub_8bit_lo)>;
def : Pat<(i16 (zext GR8:$src)),
          (INSERT_SUBREG
              (INSERT_SUBREG (i16 (IMPLICIT_DEF)), (MOV8r0), sub_8bit_hi),
              GR8:$src, sub_8bit_lo)>;

// sext i8 -> i16, done in place in a hi-half-capable register (GR16H) whose low
// byte holds the value.  Two-address; expanded post-RA (see expandPostRAPseudo)
// to `cbw` when it landed in AX, else `xchg ax,r; cbw; xchg ax,r` -- one byte vs
// three, but the xchg form uses AX only transiently (swapping any live value out
// and back), so it avoids spilling AX when it is live and, unlike a rotate
// sequence, leaves FLAGS untouched.  The allocator (AX-first order) picks AX when
// free.  No Defs=[FLAGS]: neither cbw nor xchg touch the flags.
let isCodeGenOnly = 1, Constraints = "$src = $dst" in
def SEXT16 : I8086Pseudo<(outs GR16H:$dst), (ins GR16H:$src), "#SEXT16", []>;

def : Pat<(sext GR8:$src),
          (SEXT16 (INSERT_SUBREG (i16 (IMPLICIT_DEF)), GR8:$src, sub_8bit_lo))>;

// A 16-bit shift/rotate by 8..15 is a byte op plus a residual shift of 0..7 --
// vastly faster than the CL form (shift-by-CL is 8+4*count cycles).  The custom
// inserter brings the value through AX, does the byte op (mov ah,al / xchg al,ah
// / cbw / ...), then the small residual shift.  $extra = count - 8.
def ShHiExtra : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(N->getZExtValue() - 8, SDLoc(N), MVT::i8);
}]>;
def shamt8to15 : PatLeaf<(i8 imm), [{
  int64_t V = N->getZExtValue();
  return V >= 8 && V <= 15;
}], ShHiExtra>;
// The value is brought through AX for the byte op, and the residual shift of
// 3..7 (or the flags-live fallback) uses CL, so both AX and CL are clobbered and
// must be declared -- see the note on the shift pseudos above.
let isCodeGenOnly = 1, usesCustomInserter = 1, Defs = [AX, CL, FLAGS] in {
  def Shl16Hi : I8086Pseudo<(outs GR16:$dst), (ins GR16:$src, i8imm:$extra), "#Shl16Hi", []>;
  def Shr16Hi : I8086Pseudo<(outs GR16:$dst), (ins GR16:$src, i8imm:$extra), "#Shr16Hi", []>;
  def Sar16Hi : I8086Pseudo<(outs GR16:$dst), (ins GR16:$src, i8imm:$extra), "#Sar16Hi", []>;
  def Rol16by8 : I8086Pseudo<(outs GR16:$dst), (ins GR16:$src), "#Rol16by8",
                             [(set GR16:$dst, (rotl GR16:$src, (i8 8)))]>;
  def Ror16by8 : I8086Pseudo<(outs GR16:$dst), (ins GR16:$src), "#Ror16by8",
                             [(set GR16:$dst, (rotr GR16:$src, (i8 8)))]>;
}
def : Pat<(shl GR16:$x, shamt8to15:$n), (Shl16Hi GR16:$x, shamt8to15:$n)>;
def : Pat<(srl GR16:$x, shamt8to15:$n), (Shr16Hi GR16:$x, shamt8to15:$n)>;
def : Pat<(sra GR16:$x, shamt8to15:$n), (Sar16Hi GR16:$x, shamt8to15:$n)>;
// Result truncated to a byte (the high byte is dead): just take the high byte,
// with no zeroing or sign extension.
def : Pat<(i8 (trunc (srl GR16:$x, (i8 8)))), (EXTRACT_SUBREG GR16:$x, sub_8bit_hi)>;
def : Pat<(i8 (trunc (sra GR16:$x, (i8 8)))), (EXTRACT_SUBREG GR16:$x, sub_8bit_hi)>;
