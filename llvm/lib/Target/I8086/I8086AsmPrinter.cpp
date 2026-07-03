//===-- I8086AsmPrinter.cpp - I8086 LLVM assembly writer -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8086.h"
#include "I8086InstrInfo.h"
#include "I8086MCInstLower.h"
#include "I8086TargetMachine.h"
#include "MCTargetDesc/I8086InstPrinter.h"
#include "TargetInfo/I8086TargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// Annotate each emitted instruction with its estimated 8086 size and cost as a
// trailing comment (needs a verbose-asm output, e.g. llc -S).  A hand-analysis
// aid for the size/cycle-tuned backend.
static cl::opt<bool> AnnotateCost(
    "i8086-annotate-cost", cl::Hidden,
    cl::desc("Annotate each i8086 instruction with its estimated size/cost"));

namespace {
class I8086AsmPrinter : public AsmPrinter {
public:
  I8086AsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}

  StringRef getPassName() const override { return "I8086 Assembly Printer"; }

  void printOperand(const MachineInstr *MI, int OpNum, raw_ostream &O);
  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &O) override;
  void emitInstruction(const MachineInstr *MI) override;

  static char ID;
};
} // namespace

void I8086AsmPrinter::printOperand(const MachineInstr *MI, int OpNum,
                                   raw_ostream &O) {
  const MachineOperand &MO = MI->getOperand(OpNum);
  switch (MO.getType()) {
  default:
    llvm_unreachable("Not implemented yet!");
  case MachineOperand::MO_Register:
    O << I8086InstPrinter::getRegisterName(MO.getReg());
    return;
  case MachineOperand::MO_Immediate:
    O << MO.getImm();
    return;
  case MachineOperand::MO_MachineBasicBlock:
    MO.getMBB()->getSymbol()->print(O, MAI);
    return;
  case MachineOperand::MO_GlobalAddress:
    getSymbol(MO.getGlobal())->print(O, MAI);
    return;
  }
}

bool I8086AsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                      const char *ExtraCode, raw_ostream &O) {
  if (ExtraCode && ExtraCode[0])
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, O);
  printOperand(MI, OpNo, O);
  return false;
}

void I8086AsmPrinter::emitInstruction(const MachineInstr *MI) {
  if (AnnotateCost && OutStreamer->isVerboseAsm()) {
    const auto *TII =
        static_cast<const I8086InstrInfo *>(MF->getSubtarget().getInstrInfo());
    OutStreamer->AddComment("cost: " + Twine(TII->getInstructionCost(*MI)) +
                            " cyc, " + Twine(TII->getInstSizeInBytes(*MI)) +
                            " B");
  }

  I8086MCInstLower MCInstLowering(OutContext, *this);
  MCInst TmpInst;
  MCInstLowering.Lower(MI, TmpInst);
  EmitToStreamer(*OutStreamer, TmpInst);
}

char I8086AsmPrinter::ID = 0;

INITIALIZE_PASS(I8086AsmPrinter, "i8086-asm-printer", "I8086 Assembly Printer",
                false, false)

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeI8086AsmPrinter() {
  RegisterAsmPrinter<I8086AsmPrinter> X(getTheI8086Target());
}
