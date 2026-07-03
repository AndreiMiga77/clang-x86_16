//===-- I8086TargetMachine.cpp - Define TargetMachine for I8086 ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "I8086TargetMachine.h"
#include "I8086.h"
#include "I8086MachineFunctionInfo.h"
#include "TargetInfo/I8086TargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include <optional>
using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeI8086Target() {
  RegisterTargetMachine<I8086TargetMachine> X(getTheI8086Target());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeI8086DAGToDAGISelLegacyPass(PR);
  initializeI8086AsmPrinterPass(PR);
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

I8086TargetMachine::I8086TargetMachine(const Target &T, const Triple &TT,
                                       StringRef CPU, StringRef FS,
                                       const TargetOptions &Options,
                                       std::optional<Reloc::Model> RM,
                                       std::optional<CodeModel::Model> CM,
                                       CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, TT.computeDataLayout(), TT, CPU, FS, Options,
                               getEffectiveRelocModel(RM),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()),
      Subtarget(TT, std::string(CPU), std::string(FS), *this) {
  initAsmInfo();
}

I8086TargetMachine::~I8086TargetMachine() = default;

namespace {
class I8086PassConfig : public TargetPassConfig {
public:
  I8086PassConfig(I8086TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  I8086TargetMachine &getI8086TargetMachine() const {
    return getTM<I8086TargetMachine>();
  }

  bool addInstSelector() override;
  void addPreEmitPass() override;
};
} // namespace

TargetPassConfig *I8086TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new I8086PassConfig(*this, PM);
}

MachineFunctionInfo *I8086TargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return I8086MachineFunctionInfo::create<I8086MachineFunctionInfo>(Allocator, F,
                                                                    STI);
}

bool I8086PassConfig::addInstSelector() {
  addPass(createI8086ISelDag(getI8086TargetMachine(), getOptLevel()));
  return false;
}

void I8086PassConfig::addPreEmitPass() {
  // Post-RA encoding fixups.  Both must run before branch relaxation so the
  // instruction sizes they change are final when offsets are computed.  The
  // byte-mask pass runs first so `and ax,0xFF` becomes `xor ah,ah` rather than
  // the (longer) accumulator AND.
  addPass(createI8086FixupByteMaskPass());
  addPass(createI8086CompactEncodingPass());
  // Relax out-of-range rel8 conditional branches into inverted-Jcc + JMP16.
  addPass(&BranchRelaxationPassID);
}
