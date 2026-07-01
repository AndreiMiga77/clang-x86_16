//===- I8086.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The Intel 8086 is a 16-bit processor with a 20-bit segmented address space.
// This backend links flat 16-bit images (typically DOS .COM programs, placed at
// offset 0x100 within a single segment) and resolves the four 8086 relocation
// types emitted by the LLVM i8086 assembler.
//
//===----------------------------------------------------------------------===//

#include "Symbols.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class I8086 final : public TargetInfo {
public:
  I8086(Ctx &);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};
} // namespace

I8086::I8086(Ctx &ctx) : TargetInfo(ctx) {
  // int3 (0xCC) is a reasonable trap fill.
  trapInstr = {0xCC, 0xCC, 0xCC, 0xCC};
}

RelExpr I8086::getRelExpr(RelType type, const Symbol &s,
                          const uint8_t *loc) const {
  switch (type) {
  case R_I8086_16_PCREL:
  case R_I8086_8_PCREL:
    return R_PC;
  default:
    return R_ABS;
  }
}

void I8086::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_I8086_8:
    checkIntUInt(ctx, loc, val, 8, rel);
    *loc = val & 0xff;
    break;
  case R_I8086_16:
    checkIntUInt(ctx, loc, val, 16, rel);
    write16le(loc, val & 0xffff);
    break;
  case R_I8086_8_PCREL: {
    // Branch displacement is measured from the end of the 1-byte field.
    int64_t disp = int64_t(val) - 1;
    checkInt(ctx, loc, disp, 8, rel);
    *loc = disp & 0xff;
    break;
  }
  case R_I8086_16_PCREL: {
    // Branch displacement is measured from the end of the 2-byte field.
    int64_t disp = int64_t(val) - 2;
    checkInt(ctx, loc, disp, 16, rel);
    write16le(loc, disp & 0xffff);
    break;
  }
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized relocation " << rel.type;
  }
}

void elf::setI8086TargetInfo(Ctx &ctx) { ctx.target.reset(new I8086(ctx)); }
