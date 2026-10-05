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
#include "OutputSections.h"
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
  void relocateAlloc(InputSection &sec, uint8_t *buf) const override;

private:
  void checkSection(InputSection &sec, uint64_t addr) const;

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
  // A link without a script starts at the first block. The default image
  // base depends on the processor, known only now; the driver reads it after
  // this call.
  if (!cpu.empty())
    const_cast<SHARCTarget *>(this)->defaultImageBase =
        SHARC::blocks(cpu).front().ByteBase;
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

// The symbol a relocation refers to, or its section for a section symbol.
static std::string target(const Relocation &rel) {
  if (!rel.sym->getName().empty())
    return rel.sym->getName().str();
  if (auto *d = dyn_cast<Defined>(rel.sym))
    if (d->section)
      return d->section->name.str();
  return "<unnamed>";
}

// The address kind of an absolute relocation's field.
static SHARC::AddrKind addrKind(RelType type) {
  switch (type) {
  case R_SHARC_ADDR24_NW:
    return SHARC::AddrKind::ISA;
  case R_SHARC_ADDR24_SW:
    return SHARC::AddrKind::SWCode;
  case R_SHARC_ADDR32_NW:
    return SHARC::AddrKind::NW;
  case R_SHARC_ADDR32_SW:
    return SHARC::AddrKind::SWData;
  case R_SHARC_ADDR32_LW:
    return SHARC::AddrKind::LW;
  default:
    return SHARC::AddrKind::Byte;
  }
}

static StringRef kindName(SHARC::AddrKind k) {
  static const char *const names[] = {"long-word",       "normal-word",
                                      "48-bit instruction", "short-word code",
                                      "short-word data", "byte"};
  return names[unsigned(k)];
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
      Err(ctx) << getErrorLoc(ctx, loc) << rel.type << " against " << target(rel)
               << ": target is not on an instruction boundary";
      return;
    }
    checkInt(ctx, loc, off / unit, width, rel);
    SHARC::insertField(loc, segs, uint64_t(off / unit));
    return;
  }
  if (segs.empty()) {
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized relocation "
             << rel.type;
    return;
  }

  // An absolute field holds the word address of S + A in the kind of its
  // relocation, through the block containing it.
  uint64_t addr = uint32_t(val);
  SHARC::AddrKind kind = addrKind(rel.type);
  const SHARC::Block *b = SHARC::findBlock(cpu, addr);
  if (!b) {
    Err(ctx) << getErrorLoc(ctx, loc) << rel.type << " against " << target(rel)
             << ": address 0x" << utohexstr(addr)
             << " is outside the memory blocks of " << cpu;
    return;
  }
  if (b->WordBase[unsigned(kind)] == SHARC::NoView) {
    Err(ctx) << getErrorLoc(ctx, loc) << rel.type << " against " << target(rel)
             << ": " << b->Name << " has no " << kindName(kind) << " view";
    return;
  }
  std::optional<uint64_t> word = SHARC::toWord(*b, kind, addr);
  if (!word) {
    Err(ctx) << getErrorLoc(ctx, loc) << rel.type << " against " << target(rel)
             << ": address 0x" << utohexstr(addr) << " is not a "
             << kindName(kind) << " address in " << b->Name;
    return;
  }
  checkUInt(ctx, loc, *word, width, rel);
  SHARC::insertField(loc, segs, *word);
}

// A code section is decoded in one mode, chosen by its output section's
// SHF_SHARC_VISA flag, and 48-bit code must sit on 48-bit rows of its block.
void SHARCTarget::checkSection(InputSection &sec, uint64_t addr) const {
  if (!(sec.flags & SHF_EXECINSTR) || sec.getSize() == 0)
    return;
  bool visa = sec.flags & SHF_SHARC_VISA;
  OutputSection *osec = sec.getParent();
  auto firstCode = [&]() -> InputSection * {
    for (SectionCommand *cmd : osec->commands)
      if (auto *isd = dyn_cast<InputSectionDescription>(cmd))
        for (InputSection *isec : isd->sections)
          if (isec->flags & SHF_EXECINSTR)
            return isec;
    return nullptr;
  };
  if (InputSection *first = firstCode();
      first && bool(first->flags & SHF_SHARC_VISA) != visa)
    Err(ctx) << "output section " << osec->name << " mixes VISA ("
             << (visa ? sec.name : first->name) << ") and ISA code";
  if (visa)
    return;
  const SHARC::Block *b = SHARC::findBlock(cpu, addr);
  if (b && !SHARC::toWord(*b, SHARC::AddrKind::ISA, addr))
    Err(ctx) << "section " << sec.name
             << " holds 48-bit code but starts at 0x" << utohexstr(addr)
             << ", which is not a 48-bit instruction address of " << b->Name
             << "; place it with a linker script (see SHARCUsage.rst, "
                "\"Linking\")";
}

// PC-relative offsets count instructions or parcels, which is only meaningful
// within one memory block: check that the target is in P's block.
void SHARCTarget::relocateAlloc(InputSection &sec, uint8_t *buf) const {
  uint64_t secAddr = sec.getOutputSection()->addr + sec.outSecOff;
  checkSection(sec, secAddr);
  for (const Relocation &rel : sec.relocs()) {
    uint8_t *loc = buf + rel.offset;
    uint64_t p = secAddr + rel.offset;
    uint64_t val = SignExtend64(sec.getRelocTargetVA(ctx, rel, p), 32);
    if (SHARC::isPCRelReloc(rel.type)) {
      const SHARC::Block *from = SHARC::findBlock(cpu, p);
      const SHARC::Block *to = SHARC::findBlock(cpu, uint32_t(p + val));
      if (from != to) {
        Err(ctx) << getErrorLoc(ctx, loc) << rel.type << " against "
                 << target(rel) << ": branch crosses from "
                 << (from ? from->Name : StringRef("unmapped memory"))
                 << " to " << (to ? to->Name : StringRef("unmapped memory"));
        continue;
      }
    }
    relocate(loc, rel, val);
  }
}

void elf::setSHARCTargetInfo(Ctx &ctx) { ctx.target.reset(new SHARCTarget(ctx)); }
