//===-- SHARCParcels.h - SHARC instruction byte layout ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Instructions are stored as 16-bit little-endian parcels, most significant
// parcel first (llvm/docs/SHARCUsage.rst, "Object file layout"). A 32- or
// 16-bit VISA instruction is the top two or one parcels of a 48-bit window.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCPARCELS_H
#define LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCPARCELS_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>

namespace llvm::SHARC {

/// Read up to three parcels into a 48-bit window, instruction left-aligned at
/// bit 47. \p Avail is set to the number of parcels actually present.
inline uint64_t readWindow(ArrayRef<uint8_t> Bytes, unsigned &Avail) {
  uint64_t W = 0;
  Avail = 0;
  for (unsigned I = 0; I < 3; ++I) {
    uint64_t P = 0;
    if (Bytes.size() >= 2 * I + 2) {
      P = Bytes[2 * I] | (uint64_t(Bytes[2 * I + 1]) << 8);
      ++Avail;
    }
    W |= P << (32 - 16 * I);
  }
  return W;
}

/// Append the top \p Width bits of a 48-bit window as parcels.
inline void writeParcels(uint64_t Window48, unsigned Width,
                         SmallVectorImpl<char> &CB) {
  for (unsigned I = 0; I < Width / 16; ++I) {
    uint16_t P = (Window48 >> (32 - 16 * I)) & 0xFFFF;
    CB.push_back(char(P & 0xFF));
    CB.push_back(char(P >> 8));
  }
}

} // namespace llvm::SHARC

#endif // LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCPARCELS_H
