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
// Subtarget features.  These are *tuning* (microarchitecture) properties, not
// instruction-set ones: the 8088 and 8086 share an ISA and differ only in the
// bus width, so they are distinguished purely by tuning.  (ISA-level features
// for the 80186+ extra instructions would be separate -march features.)
//===----------------------------------------------------------------------===//

// 16-bit data/prefetch bus (8086, 80186).  The 8088 has an 8-bit bus, so it
// prefetches at 4 clocks/byte and pays an extra bus cycle per word memory
// access; this drives the cost model's fetch term.
def FeatureWideBus : SubtargetFeature<"wide-bus", "HasWideBus", "true",
  "Has a 16-bit data/prefetch bus (8086/80186); the 8088 bus is 8-bit">;

// Cheap effective-address calculation (80186+ compute an EA in ~2 clocks versus
// the 8086's 5-8).  Only then is folding address arithmetic into an indexed
// [base+index] memory operand a general win; on the 8086/8088 the high EA cost
// makes it worthwhile only when the index is already in SI/DI (a post-RA fold),
// so this feature gates the isel indexed-addressing patterns.
def FeatureFastEA : SubtargetFeature<"fast-ea", "HasFastEA", "true",
  "Effective-address calculation is cheap (80186+)">;

def HasFastEA : Predicate<"Subtarget->hasFastEA()">;

//===----------------------------------------------------------------------===//
// Supported processors.
//===----------------------------------------------------------------------===//
class Proc<string Name, list<SubtargetFeature> Features>
 : Processor<Name, NoItineraries, Features>;

def : Proc<"generic", [FeatureWideBus]>;
def : Proc<"i8088",   []>;
def : Proc<"i8086",   [FeatureWideBus]>;
def : Proc<"i80186",  [FeatureWideBus, FeatureFastEA]>;

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
