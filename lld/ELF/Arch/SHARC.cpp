//===- SHARC.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Analog Devices SHARC and SHARC+ (llvm/docs/SHARCUsage.rst). ELF addresses
// are byte addresses; instruction fields hold word addresses whose meaning
// depends on the memory block, converted with the per-processor table in
// llvm/TargetParser/SHARCMemoryMap.def.
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "Symbols.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/BinaryFormat/SHARCRelocFields.h"
#include "llvm/Support/Endian.h"
#include "llvm/TargetParser/SHARCMemoryMap.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
class SHARCTarget final : public TargetInfo {
public:
  SHARCTarget(Ctx &);
  uint32_t calcEFlags() const override;
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;

private:
  // The processor whose memory table resolves word addresses; set by
  // calcEFlags, which runs before any relocation is applied.
  mutable StringRef cpu;
};
} // namespace

SHARCTarget::SHARCTarget(Ctx &ctx) : TargetInfo(ctx) {
  // A zero word is a NOP (Type 21a).
  trapInstr = {0, 0, 0, 0};
}

static uint32_t getEFlags(InputFile *file) {
  return cast<ObjFile<ELF32LE>>(file)->getObj().getHeader().e_flags;
}

// Merge one e_flags field: inputs without a value are compatible with any.
static uint32_t mergeField(Ctx &ctx, uint32_t mask, StringRef what) {
  uint32_t value = 0;
  InputFile *first = nullptr;
  for (InputFile *f : ctx.objectFiles) {
    uint32_t v = getEFlags(f) & mask;
    if (!v)
      continue;
    if (!value) {
      value = v;
      first = f;
    } else if (v != value) {
      ErrAlways(ctx) << f << ": cannot link object files for different SHARC "
                     << what << " (" << first << " has 0x" << utohexstr(value)
                     << ", this file 0x" << utohexstr(v) << ")";
    }
  }
  return value;
}

uint32_t SHARCTarget::calcEFlags() const {
  uint32_t flags = mergeField(ctx, EF_SHARC_MACH, "processors") |
                   mergeField(ctx, EF_SHARC_PART, "parts");
  cpu = SHARC::memoryMapCPU(flags);
  if (cpu.empty()) {
    if ((flags & EF_SHARC_MACH) == EF_SHARC_MACH_2137X)
      ErrAlways(ctx) << "no SHARC memory map for these objects; assemble "
                        "classic code with -mcpu=adsp-21371 or "
                        "-mcpu=adsp-21375";
    else
      ErrAlways(ctx) << "no SHARC memory map for e_flags 0x"
                     << utohexstr(flags)
                     << "; supported: adsp-2156x, adsp-21371, adsp-21375";
  }
  return flags;
}

RelExpr SHARCTarget::getRelExpr(RelType type, const Symbol &s,
                          const uint8_t *loc) const {
  switch (type) {
  case R_SHARC_NONE:
    return R_NONE;
  case R_SHARC_PCREL24_ISA:
  case R_SHARC_PCREL24_VISA:
  case R_SHARC_PCREL23_ISA:
  case R_SHARC_PCREL23_VISA:
  case R_SHARC_PCREL6_ISA:
  case R_SHARC_PCREL6_VISA:
    return R_PC;
  case R_SHARC_DATA32:
  case R_SHARC_ADDR24_NW:
  case R_SHARC_ADDR24_SW:
  case R_SHARC_ADDR32_NW:
  case R_SHARC_ADDR32_SW:
  case R_SHARC_ADDR32_LW:
  case R_SHARC_ADDR32_BYTE:
    return R_ABS;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unsupported relocation " << type
             << " against symbol " << &s;
    return R_NONE;
  }
}

void SHARCTarget::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  if (rel.type == R_SHARC_DATA32) {
    checkIntUInt(ctx, loc, val, 32, rel);
    write32le(loc, val);
    return;
  }
  auto segs = SHARC::relocFieldSegments(rel.type);
  unsigned width = SHARC::fieldWidth(segs);
  if (SHARC::isPCRelReloc(rel.type)) {
    // val is S + A - P in bytes; P is the instruction's first byte.
    int64_t off = int64_t(val);
    int64_t unit = SHARC::pcRelRelocUnit(rel.type);
    if (off % unit) {
      Err(ctx) << getErrorLoc(ctx, loc) << rel.type << " against " << rel.sym
               << ": target is not on an instruction boundary";
      return;
    }
    checkInt(ctx, loc, off / unit, width, rel);
    SHARC::insertField(loc, segs, uint64_t(off / unit));
    return;
  }
  Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized relocation " << rel.type;
}

void elf::setSHARCTargetInfo(Ctx &ctx) { ctx.target.reset(new SHARCTarget(ctx)); }
