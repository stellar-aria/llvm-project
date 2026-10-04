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
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};

/// Bit ranges (hi, lo) of a fixup's field in the 48-bit instruction window,
/// most significant first.
inline ArrayRef<std::pair<unsigned, unsigned>> fixupSegments(unsigned Kind) {
  static const std::pair<unsigned, unsigned> Bits24[] = {{23, 0}};
  static const std::pair<unsigned, unsigned> Bits23[] = {{22, 0}};
  static const std::pair<unsigned, unsigned> Bits6[] = {{32, 32}, {31, 27}};
  static const std::pair<unsigned, unsigned> Bits32[] = {{31, 0}};
  switch (Kind) {
  case fixup_sharc_pcrel24_isa:
  case fixup_sharc_pcrel24_visa:
  case fixup_sharc_addr24_isa:
  case fixup_sharc_addr24_visa:
    return Bits24;
  case fixup_sharc_pcrel23_isa:
  case fixup_sharc_pcrel23_visa:
    return Bits23;
  case fixup_sharc_pcrel6_isa:
  case fixup_sharc_pcrel6_visa:
    return Bits6;
  default:
    return Bits32;
  }
}

inline bool isPCRelFixup(unsigned Kind) {
  return Kind >= fixup_sharc_pcrel24_isa && Kind <= fixup_sharc_pcrel6_visa;
}

/// Bytes per PC-relative unit: a 48-bit instruction or a 16-bit parcel.
inline unsigned pcRelUnit(unsigned Kind) {
  return (Kind - fixup_sharc_pcrel24_isa) % 2 ? 2 : 6;
}

} // namespace llvm::SHARC

#endif // LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCFIXUPKINDS_H
