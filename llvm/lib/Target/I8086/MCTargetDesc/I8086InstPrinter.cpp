//===-- I8086InstPrinter.cpp - Convert I8086 MCInst to asm syntax ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8086InstPrinter.h"
#include "I8086MCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

#include "I8086GenAsmWriter.inc"

void I8086InstPrinter::printRegName(raw_ostream &O, MCRegister Reg) {
  O << getRegisterName(Reg);
}

void I8086InstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                 StringRef Annot, const MCSubtargetInfo &STI,
                                 raw_ostream &O) {
  printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

void I8086InstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                    raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg())
    O << getRegisterName(Op.getReg());
  else if (Op.isImm())
    O << Op.getImm();
  else {
    assert(Op.isExpr() && "unknown operand kind in printOperand");
    MAI.printExpr(O, *Op.getExpr());
  }
}

void I8086InstPrinter::printImm(const MCInst *MI, unsigned OpNo,
                                raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm())
    O << Op.getImm();
  else {
    assert(Op.isExpr() && "unknown immediate operand");
    MAI.printExpr(O, *Op.getExpr());
  }
}

void I8086InstPrinter::printPCRelImm(const MCInst *MI, unsigned OpNo,
                                     raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm())
    O << Op.getImm();
  else {
    assert(Op.isExpr() && "unknown pcrel operand");
    MAI.printExpr(O, *Op.getExpr());
  }
}

void I8086InstPrinter::printMemReference(const MCInst *MI, unsigned OpNo,
                                         raw_ostream &O) {
  // Memory operand: base, index, disp, seg.
  const MCOperand &Base = MI->getOperand(OpNo);
  const MCOperand &Index = MI->getOperand(OpNo + 1);
  const MCOperand &Disp = MI->getOperand(OpNo + 2);
  const MCOperand &Seg = MI->getOperand(OpNo + 3);

  if (Seg.isReg() && Seg.getReg())
    O << getRegisterName(Seg.getReg()) << ':';

  O << '[';
  bool NeedPlus = false;
  if (Base.isReg() && Base.getReg()) {
    O << getRegisterName(Base.getReg());
    NeedPlus = true;
  }
  if (Index.isReg() && Index.getReg()) {
    if (NeedPlus)
      O << " + ";
    O << getRegisterName(Index.getReg());
    NeedPlus = true;
  }
  if (Disp.isImm()) {
    int64_t V = Disp.getImm();
    if (V != 0 || !NeedPlus) {
      if (NeedPlus) {
        if (V < 0) {
          O << " - " << -V;
        } else {
          O << " + " << V;
        }
      } else {
        O << V;
      }
    }
  } else if (Disp.isExpr()) {
    if (NeedPlus)
      O << " + ";
    MAI.printExpr(O, *Disp.getExpr());
  }
  O << ']';
}
