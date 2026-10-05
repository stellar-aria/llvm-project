//===- SHARCRelocFields.h - SHARC relocation field bits ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The bits each R_SHARC_* relocation fills, in the 48-bit instruction window
// (llvm/docs/SHARCUsage.rst, "Relocations"), and how to write them into an
// instruction stored as 16-bit little-endian parcels, most significant parcel
// first. Shared by the MC asm backend and LLD.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BINARYFORMAT_SHARCRELOCFIELDS_H
#define LLVM_BINARYFORMAT_SHARCRELOCFIELDS_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>
#include <cstdint>
#include <utility>

namespace llvm::SHARC {

/// (hi, lo) bits of a field in the 48-bit window, most significant first.
using FieldSegment = std::pair<unsigned, unsigned>;

inline ArrayRef<FieldSegment> relocFieldSegments(uint32_t Type) {
  static const FieldSegment Bits24[] = {{23, 0}};
  static const FieldSegment Bits23[] = {{22, 0}};
  static const FieldSegment Bits6[] = {{32, 32}, {31, 27}};
  static const FieldSegment Bits32[] = {{31, 0}};
  switch (Type) {
  case ELF::R_SHARC_ADDR24_NW:
  case ELF::R_SHARC_ADDR24_SW:
  case ELF::R_SHARC_PCREL24_ISA:
  case ELF::R_SHARC_PCREL24_VISA:
    return Bits24;
  case ELF::R_SHARC_PCREL23_ISA:
  case ELF::R_SHARC_PCREL23_VISA:
    return Bits23;
  case ELF::R_SHARC_PCREL6_ISA:
  case ELF::R_SHARC_PCREL6_VISA:
    return Bits6;
  case ELF::R_SHARC_ADDR32_NW:
  case ELF::R_SHARC_ADDR32_SW:
  case ELF::R_SHARC_ADDR32_LW:
  case ELF::R_SHARC_ADDR32_BYTE:
    return Bits32;
  default:
    return {};
  }
}

inline unsigned fieldWidth(ArrayRef<FieldSegment> Segs) {
  unsigned W = 0;
  for (auto [Hi, Lo] : Segs)
    W += Hi - Lo + 1;
  return W;
}

inline bool isPCRelReloc(uint32_t Type) {
  return Type >= ELF::R_SHARC_PCREL24_ISA && Type <= ELF::R_SHARC_PCREL6_VISA;
}

/// Bytes per PC-relative unit: a 48-bit instruction (ISA) or a 16-bit
/// parcel (VISA).
inline unsigned pcRelRelocUnit(uint32_t Type) {
  return (Type - ELF::R_SHARC_PCREL24_ISA) % 2 ? 2 : 6;
}

/// Scatter the low fieldWidth(Segs) bits of \p V over \p Segs of the
/// instruction at \p Insn, rewriting only the parcels the field touches.
inline void insertField(uint8_t *Insn, ArrayRef<FieldSegment> Segs,
                        uint64_t V) {
  unsigned Remaining = fieldWidth(Segs);
  for (auto [Hi, Lo] : Segs) {
    unsigned N = Hi - Lo + 1;
    Remaining -= N;
    uint64_t Part = (V >> Remaining) & maskTrailingOnes<uint64_t>(N);
    for (unsigned P = (47 - Hi) / 16; P <= (47 - Lo) / 16; ++P) {
      unsigned PLo = 32 - 16 * P; // window bit of the parcel's bit 0
      unsigned From = std::max(Lo, PLo), To = std::min(Hi, PLo + 15);
      uint16_t Mask = maskTrailingOnes<uint16_t>(To - From + 1) << (From - PLo);
      uint16_t Bits = uint16_t((Part >> (From - Lo)) << (From - PLo)) & Mask;
      uint16_t Parcel = Insn[2 * P] | (uint16_t(Insn[2 * P + 1]) << 8);
      Parcel = (Parcel & ~Mask) | Bits;
      Insn[2 * P] = uint8_t(Parcel);
      Insn[2 * P + 1] = uint8_t(Parcel >> 8);
    }
  }
}

} // namespace llvm::SHARC

#endif // LLVM_BINARYFORMAT_SHARCRELOCFIELDS_H
