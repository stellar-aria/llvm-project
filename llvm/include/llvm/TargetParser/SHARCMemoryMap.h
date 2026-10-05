//===- SHARCMemoryMap.h - SHARC memory blocks per processor -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Conversions between SHARC ELF byte addresses and the word addresses that
// instruction fields hold, per memory block (SHARCMemoryMap.def).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGETPARSER_SHARCMEMORYMAP_H
#define LLVM_TARGETPARSER_SHARCMEMORYMAP_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <optional>

namespace llvm::SHARC {

/// Address kinds of instruction fields: long word, normal word, 48-bit
/// instruction (ISA code), short word as a VISA code address, short word as
/// a data address, and byte.
enum class AddrKind { LW, NW, ISA, SWCode, SWData, Byte };

inline constexpr uint64_t NoView = ~0ULL;

struct Block {
  StringRef CPU, Name;
  uint64_t ByteBase, Size;
  uint64_t WordBase[6]; ///< Indexed by AddrKind; NoView if absent.
};

/// Bytes per word of \p K.
unsigned unitBytes(AddrKind K);

/// The blocks of \p CPU; empty if the CPU has no table.
ArrayRef<Block> blocks(StringRef CPU);

/// The block of \p CPU containing \p Byte, or null.
const Block *findBlock(StringRef CPU, uint64_t Byte);

/// The word address of \p Byte in \p B, if \p B has a view of \p K, \p Byte
/// starts a whole word of it, and the word lies inside the block.
std::optional<uint64_t> toWord(const Block &B, AddrKind K, uint64_t Byte);

/// The byte address of word \p Word of kind \p K on \p CPU.
std::optional<uint64_t> toByte(StringRef CPU, AddrKind K, uint64_t Word);

/// The CPU whose table serves objects with these ELF flags, or "" if none
/// does. EF_SHARC_MACH_SHARCPLUS uses the ADSP-2156x table; classic objects
/// need a part (EF_SHARC_PART).
StringRef memoryMapCPU(unsigned EFlags);

} // namespace llvm::SHARC

#endif // LLVM_TARGETPARSER_SHARCMEMORYMAP_H
