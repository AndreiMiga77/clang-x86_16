//===-- I8086AsmBackend.cpp - I8086 Assembler Backend --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/I8086FixupKinds.h"
#include "MCTargetDesc/I8086MCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
class I8086AsmBackend : public MCAsmBackend {
  uint8_t OSABI;

public:
  I8086AsmBackend(uint8_t OSABI)
      : MCAsmBackend(llvm::endianness::little), OSABI(OSABI) {}

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createI8086ELFObjectWriter(OSABI);
  }

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    const static MCFixupKindInfo Infos[I8086::NumTargetFixupKinds] = {
        // name             offset bits flags
        {"fixup_16",        0, 16, 0},
        {"fixup_8",         0,  8, 0},
        {"fixup_16_pcrel",  0, 16, 0},
        {"fixup_8_pcrel",   0,  8, 0},
    };
    static_assert(std::size(Infos) == I8086::NumTargetFixupKinds,
                  "Not all fixup kinds added to Infos array");

    if (Kind < FirstTargetFixupKind)
      return MCAsmBackend::getFixupKindInfo(Kind);
    return Infos[Kind - FirstTargetFixupKind];
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    // 8086 NOP is 0x90 (xchg ax, ax).
    OS.write_zeros(0);
    for (uint64_t I = 0; I != Count; ++I)
      OS << char(0x90);
    return true;
  }
};

void I8086AsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                                 const MCValue &Target, uint8_t *Data,
                                 uint64_t Value, bool IsResolved) {
  maybeAddReloc(F, Fixup, Target, Value, IsResolved);
  if (!IsResolved)
    return;

  unsigned Kind = Fixup.getKind();
  unsigned NumBytes = 0;
  switch (Kind) {
  case FK_Data_1:
  case I8086::fixup_8:
    NumBytes = 1;
    break;
  case FK_Data_2:
  case I8086::fixup_16:
    NumBytes = 2;
    break;
  case I8086::fixup_8_pcrel:
    // Displacement is relative to the end of the 1-byte field.
    Value -= 1;
    NumBytes = 1;
    break;
  case I8086::fixup_16_pcrel:
    // Displacement is relative to the end of the 2-byte field.
    Value -= 2;
    NumBytes = 2;
    break;
  default:
    return;
  }

  // Data already points at the fixup location within the fragment.
  for (unsigned I = 0; I != NumBytes; ++I)
    Data[I] = uint8_t((Value >> (I * 8)) & 0xff);
}

} // namespace

MCAsmBackend *llvm::createI8086MCAsmBackend(const Target &T,
                                            const MCSubtargetInfo &STI,
                                            const MCRegisterInfo &MRI,
                                            const MCTargetOptions &Options) {
  uint8_t OSABI = ELF::ELFOSABI_STANDALONE;
  return new I8086AsmBackend(OSABI);
}
