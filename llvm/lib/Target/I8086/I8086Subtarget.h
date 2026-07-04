//===-- I8086Subtarget.h - Define Subtarget for the I8086 -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_I8086_I8086SUBTARGET_H
#define LLVM_LIB_TARGET_I8086_I8086SUBTARGET_H

#include "I8086FrameLowering.h"
#include "I8086ISelLowering.h"
#include "I8086InstrInfo.h"
#include "I8086RegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include <string>

#define GET_SUBTARGETINFO_HEADER
#include "I8086GenSubtargetInfo.inc"

namespace llvm {
class StringRef;

class I8086Subtarget : public I8086GenSubtargetInfo {
  virtual void anchor();

  // Tuning feature bits (set by ParseSubtargetFeatures from the CPU/features).
  bool HasWideBus = false;
  bool HasFastEA = false;

  I8086InstrInfo InstrInfo;
  I8086TargetLowering TLInfo;
  std::unique_ptr<const SelectionDAGTargetInfo> TSInfo;
  I8086FrameLowering FrameLowering;

public:
  // 16-bit bus (8086/80186) fetches 2 bytes per 4-clock cycle; the 8088's 8-bit
  // bus fetches 1 byte, and pays an extra bus cycle per word memory access.
  bool hasWideBus() const { return HasWideBus; }
  // 80186+ effective-address calculation is cheap enough that indexed
  // [base+index] addressing is generally profitable.
  bool hasFastEA() const { return HasFastEA; }

  I8086Subtarget(const Triple &TT, const std::string &CPU,
                 const std::string &FS, const TargetMachine &TM);
  ~I8086Subtarget() override;

  I8086Subtarget &initializeSubtargetDependencies(StringRef CPU, StringRef FS);

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  const TargetFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const I8086InstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const I8086RegisterInfo *getRegisterInfo() const override {
    return &getInstrInfo()->getRegisterInfo();
  }
  const I8086TargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return TSInfo.get();
  }
};
} // namespace llvm

#endif // LLVM_LIB_TARGET_I8086_I8086SUBTARGET_H
