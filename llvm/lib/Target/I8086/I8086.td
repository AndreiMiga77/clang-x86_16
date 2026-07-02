//===-- I8086.td - Describe the I8086 Target Machine -------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This is the top level entry point for the Intel 8086 target.  It is an
// assembler-only target (no CodeGen), used to emit 16-bit 8086 machine code and
// DOS .COM executables.
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Supported processors.
//===----------------------------------------------------------------------===//
class Proc<string Name, list<SubtargetFeature> Features>
 : Processor<Name, NoItineraries, Features>;

def : Proc<"generic", []>;
def : Proc<"i8086",   []>;

//===----------------------------------------------------------------------===//
// Register File, Instruction Descriptions
//===----------------------------------------------------------------------===//

include "I8086RegisterInfo.td"
include "I8086CallingConv.td"
include "I8086InstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<GR16>;

def I8086InstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Assembly Printers / Parsers
//===----------------------------------------------------------------------===//

def I8086AsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
  int PassSubtarget = 0;
  int Variant = 0;
}

def I8086AsmParser : AsmParser {
  let AllowDuplicateRegisterNames = 1;
}

def I8086AsmParserVariant : AsmParserVariant {
  int Variant = 0;
  // Recognize Intel-style memory operands: [base + index + disp].
  string RegisterPrefix = "";
}

//===----------------------------------------------------------------------===//
// Target Declaration
//===----------------------------------------------------------------------===//

def I8086 : Target {
  let InstructionSet = I8086InstrInfo;
  let AssemblyWriters = [I8086AsmWriter];
  let AssemblyParsers = [I8086AsmParser];
  let AssemblyParserVariants = [I8086AsmParserVariant];
}
