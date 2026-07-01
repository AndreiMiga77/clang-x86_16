//===-- I8086MCCodeEmitter.cpp - Encode I8086 instructions ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This implements the hand-written 8086 machine code emitter.  It interprets
// the encoding "form", immediate kind and opcode packed into TSFlags (see
// I8086BaseInfo.h / I8086InstrFormats.td) and produces the ModR/M byte,
// displacement and immediate bytes for each instruction.
//
//===----------------------------------------------------------------------===//

#include "I8086BaseInfo.h"
#include "MCTargetDesc/I8086FixupKinds.h"
#include "MCTargetDesc/I8086MCTargetDesc.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "mccodeemitter"

namespace {

class I8086MCCodeEmitter : public MCCodeEmitter {
  MCContext &Ctx;
  const MCInstrInfo &MCII;

public:
  I8086MCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx)
      : Ctx(Ctx), MCII(MCII) {}

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

private:
  unsigned regEnc(const MCInst &MI, unsigned OpNo) const {
    return Ctx.getRegisterInfo()->getEncodingValue(MI.getOperand(OpNo).getReg());
  }

  // Emit the ModR/M byte and displacement for a memory operand whose four
  // sub-operands begin at MemOp.  RegField is the value of the reg field.
  void encodeMemory(const MCInst &MI, unsigned MemOp, unsigned RegField,
                    SmallVectorImpl<char> &CB,
                    SmallVectorImpl<MCFixup> &Fixups) const;

  // Emit an absolute or pc-relative immediate stored in operand OpNo.
  void encodeImm(const MCInst &MI, unsigned OpNo, unsigned ImmType,
                 SmallVectorImpl<char> &CB,
                 SmallVectorImpl<MCFixup> &Fixups) const;
};

static void emitByte(uint8_t B, SmallVectorImpl<char> &CB) { CB.push_back(B); }

static void emitWord(uint16_t W, SmallVectorImpl<char> &CB) {
  CB.push_back(char(W & 0xff));
  CB.push_back(char((W >> 8) & 0xff));
}

// Segment override prefix byte for a segment register, or 0 if none.
static uint8_t segOverride(MCRegister Seg) {
  switch (Seg.id()) {
  case I8086::ES: return 0x26;
  case I8086::CS: return 0x2E;
  case I8086::SS: return 0x36;
  case I8086::DS: return 0x3E;
  default:        return 0;
  }
}

} // namespace

void I8086MCCodeEmitter::encodeImm(const MCInst &MI, unsigned OpNo,
                                   unsigned ImmType, SmallVectorImpl<char> &CB,
                                   SmallVectorImpl<MCFixup> &Fixups) const {
  const MCOperand &Op = MI.getOperand(OpNo);
  bool Is16 = (ImmType == I8086II::Imm16 || ImmType == I8086II::Rel16);
  bool PCRel = (ImmType == I8086II::Rel8 || ImmType == I8086II::Rel16);
  unsigned Size = Is16 ? 2 : 1;

  if (Op.isImm()) {
    if (Is16)
      emitWord(uint16_t(Op.getImm()), CB);
    else
      emitByte(uint8_t(Op.getImm()), CB);
    return;
  }

  assert(Op.isExpr() && "expected immediate expression");
  MCFixupKind Kind;
  if (PCRel)
    Kind = Is16 ? MCFixupKind(I8086::fixup_16_pcrel)
                : MCFixupKind(I8086::fixup_8_pcrel);
  else
    Kind = Is16 ? MCFixupKind(I8086::fixup_16) : MCFixupKind(I8086::fixup_8);

  Fixups.push_back(
      MCFixup::create(CB.size(), Op.getExpr(), Kind, /*PCRel=*/PCRel));
  for (unsigned I = 0; I != Size; ++I)
    emitByte(0, CB);
}

void I8086MCCodeEmitter::encodeMemory(const MCInst &MI, unsigned MemOp,
                                      unsigned RegField,
                                      SmallVectorImpl<char> &CB,
                                      SmallVectorImpl<MCFixup> &Fixups) const {
  MCRegister Base = MI.getOperand(MemOp).getReg();
  MCRegister Index = MI.getOperand(MemOp + 1).getReg();
  const MCOperand &Disp = MI.getOperand(MemOp + 2);

  unsigned RM;
  bool Direct = false;
  if (Base == I8086::BX && Index == I8086::SI) RM = 0;
  else if (Base == I8086::BX && Index == I8086::DI) RM = 1;
  else if (Base == I8086::BP && Index == I8086::SI) RM = 2;
  else if (Base == I8086::BP && Index == I8086::DI) RM = 3;
  else if (!Base && Index == I8086::SI) RM = 4;
  else if (!Base && Index == I8086::DI) RM = 5;
  else if (Base == I8086::BP && !Index) RM = 6;
  else if (Base == I8086::BX && !Index) RM = 7;
  else { RM = 6; Direct = true; } // [disp16]

  // Decide the mod field and displacement size.
  unsigned Mod;
  unsigned DispSize; // 0, 1, or 2
  bool DispIsExpr = Disp.isExpr();
  int64_t DispVal = Disp.isImm() ? Disp.getImm() : 0;

  if (Direct) {
    Mod = 0;
    DispSize = 2;
  } else if (DispIsExpr) {
    Mod = 2;
    DispSize = 2;
  } else if (DispVal == 0 && RM != 6) {
    Mod = 0;
    DispSize = 0;
  } else if (DispVal >= -128 && DispVal <= 127) {
    Mod = 1;
    DispSize = 1;
  } else {
    Mod = 2;
    DispSize = 2;
  }

  emitByte(uint8_t((Mod << 6) | ((RegField & 7) << 3) | (RM & 7)), CB);

  if (DispSize == 0)
    return;

  if (DispIsExpr) {
    // Only 16-bit displacements can carry a relocation.
    Fixups.push_back(MCFixup::create(CB.size(), Disp.getExpr(),
                                     MCFixupKind(I8086::fixup_16)));
    emitWord(0, CB);
  } else if (DispSize == 1) {
    emitByte(uint8_t(DispVal), CB);
  } else {
    emitWord(uint16_t(DispVal), CB);
  }
}

void I8086MCCodeEmitter::encodeInstruction(const MCInst &MI,
                                           SmallVectorImpl<char> &CB,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  const MCInstrDesc &Desc = MCII.get(MI.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;
  unsigned Form = I8086II::getForm(TSFlags);
  unsigned ImmType = I8086II::getImmType(TSFlags);
  uint8_t Opcode = I8086II::getOpcode(TSFlags);

  // Build a logical operand list that skips any operand tied to an earlier one
  // (the duplicated source of a two-address codegen instruction).  For the
  // assembler, which never produces ties, this is just every operand, so the
  // encoding is unchanged.
  SmallVector<unsigned, 8> Ops;
  for (unsigned I = 0, E = MI.getNumOperands(); I != E; ++I) {
    if (Desc.getOperandConstraint(I, MCOI::TIED_TO) != -1)
      continue;
    Ops.push_back(I);
  }
  auto Reg = [&](unsigned LogicalIdx) { return regEnc(MI, Ops[LogicalIdx]); };

  // Work out where a memory operand (if any) starts, so we can emit a segment
  // override prefix ahead of the opcode.
  int MemOp = -1;
  switch (Form) {
  case I8086II::MRMDestMem: MemOp = Ops[0]; break;
  case I8086II::MRMSrcMem:  MemOp = Ops[1]; break;
  default:
    if (I8086II::isMRMGroup(Form) && !I8086II::isMRMGroupReg(Form))
      MemOp = Ops[0];
    break;
  }
  if (MemOp >= 0) {
    MCRegister Seg = MI.getOperand(MemOp + 3).getReg();
    if (uint8_t P = segOverride(Seg))
      emitByte(P, CB);
  }

  // Emit the opcode byte (AddRegFrm folds the register number into it).
  if (Form == I8086II::AddRegFrm)
    emitByte(Opcode | (Reg(0) & 7), CB);
  else
    emitByte(Opcode, CB);

  // Emit ModR/M + displacement.
  switch (Form) {
  case I8086II::MRMDestReg:
    emitByte(uint8_t(0xC0 | ((Reg(1) & 7) << 3) | (Reg(0) & 7)), CB);
    break;
  case I8086II::MRMSrcReg:
    emitByte(uint8_t(0xC0 | ((Reg(0) & 7) << 3) | (Reg(1) & 7)), CB);
    break;
  case I8086II::MRMDestMem:
    encodeMemory(MI, Ops[0], Reg(4), CB, Fixups);
    break;
  case I8086II::MRMSrcMem:
    encodeMemory(MI, Ops[1], Reg(0), CB, Fixups);
    break;
  default:
    if (I8086II::isMRMGroup(Form)) {
      unsigned Ext = I8086II::getMRMExtension(Form);
      if (I8086II::isMRMGroupReg(Form))
        emitByte(uint8_t(0xC0 | (Ext << 3) | (Reg(0) & 7)), CB);
      else
        encodeMemory(MI, Ops[0], Ext, CB, Fixups);
    }
    break;
  }

  // Far pointer: opcode + off16 + seg16 (two immediate operands).
  if (Form == I8086II::RawFrmFar) {
    encodeImm(MI, Ops[0], I8086II::Imm16, CB, Fixups);
    encodeImm(MI, Ops[1], I8086II::Imm16, CB, Fixups);
    return;
  }

  // Trailing immediate (always the last logical operand when present).
  if (ImmType != I8086II::NoImm)
    encodeImm(MI, Ops.back(), ImmType, CB, Fixups);
}

MCCodeEmitter *llvm::createI8086MCCodeEmitter(const MCInstrInfo &MCII,
                                              MCContext &Ctx) {
  return new I8086MCCodeEmitter(MCII, Ctx);
}
