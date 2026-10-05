//===-- SHARCFixupKinds.h - SHARC fixup kinds -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A fixup kind names a field layout and, for code addresses and PC-relative
// offsets, the section mode: ISA offsets count 48-bit instructions and ISA
// code addresses are normal-word; VISA offsets count 16-bit parcels and VISA
// code addresses are short-word. Every fixup is placed at the start of its
// instruction, so a PC-relative value is relative to the instruction.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCFIXUPKINDS_H
#define LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCFIXUPKINDS_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/BinaryFormat/SHARCRelocFields.h"
#include "llvm/MC/MCFixup.h"
#include <utility>

namespace llvm::SHARC {

enum Fixups {
  fixup_sharc_pcrel24_isa = FirstTargetFixupKind,
  fixup_sharc_pcrel24_visa,
  fixup_sharc_pcrel23_isa,
  fixup_sharc_pcrel23_visa,
  fixup_sharc_pcrel6_isa,
  fixup_sharc_pcrel6_visa,
  fixup_sharc_addr24_isa,
  fixup_sharc_addr24_visa,
  fixup_sharc_addr32,
  /// Hardware loops (no bits, no relocation): the value is the number of
  /// instructions from this one to the end of its loop. An instruction with a
  /// 48-bit alternative relaxes to it within the last N; one without is an
  /// error there (PGR 4-54, PRM p6619 text).
  fixup_sharc_loop_end,
  fixup_sharc_loop_end_fixed,
  /// The SHARC+ loop mode bit (bit 23 of Type 12a), known when the loop
  /// closes.
  fixup_sharc_loop_mode,
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};

/// Bit ranges (hi, lo) of a fixup's field in the 48-bit instruction window,
/// most significant first (the relocation's field, SHARCRelocFields.h).
inline ArrayRef<std::pair<unsigned, unsigned>> fixupSegments(unsigned Kind) {
  static const std::pair<unsigned, unsigned> Bit23[] = {{23, 23}};
  switch (Kind) {
  case fixup_sharc_pcrel24_isa:
  case fixup_sharc_pcrel24_visa:
  case fixup_sharc_addr24_isa:
  case fixup_sharc_addr24_visa:
    return relocFieldSegments(ELF::R_SHARC_ADDR24_NW);
  case fixup_sharc_pcrel23_isa:
  case fixup_sharc_pcrel23_visa:
    return relocFieldSegments(ELF::R_SHARC_PCREL23_ISA);
  case fixup_sharc_pcrel6_isa:
  case fixup_sharc_pcrel6_visa:
    return relocFieldSegments(ELF::R_SHARC_PCREL6_ISA);
  case fixup_sharc_loop_mode:
    return Bit23;
  case fixup_sharc_loop_end:
  case fixup_sharc_loop_end_fixed:
    return {};
  default:
    return relocFieldSegments(ELF::R_SHARC_ADDR32_NW);
  }
}

inline bool isPCRelFixup(unsigned Kind) {
  return Kind >= fixup_sharc_pcrel24_isa && Kind <= fixup_sharc_pcrel6_visa;
}

/// How many instructions at the end of a hardware loop must be 48-bit.
inline unsigned loopEndInstructions(bool SHARCPlus) { return SHARCPlus ? 11 : 4; }

/// Bytes per PC-relative unit: a 48-bit instruction or a 16-bit parcel.
inline unsigned pcRelUnit(unsigned Kind) {
  return (Kind - fixup_sharc_pcrel24_isa) % 2 ? 2 : 6;
}

} // namespace llvm::SHARC

#endif // LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCFIXUPKINDS_H
