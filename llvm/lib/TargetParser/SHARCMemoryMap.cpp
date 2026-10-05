//===- SHARCMemoryMap.cpp - SHARC memory blocks per processor -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/TargetParser/SHARCMemoryMap.h"
#include "llvm/BinaryFormat/ELF.h"

using namespace llvm;
using namespace llvm::SHARC;

static const Block Blocks[] = {
#define SHARC_BLOCK(CPU, NAME, BYTE, SIZE, LW, NW, ISA, SWC, SWD, B)           \
  {CPU, NAME, BYTE, SIZE, {LW, NW, ISA, SWC, SWD, B}},
#include "llvm/TargetParser/SHARCMemoryMap.def"
};

unsigned SHARC::unitBytes(AddrKind K) {
  static const unsigned Units[] = {8, 4, 6, 2, 2, 1};
  return Units[unsigned(K)];
}

ArrayRef<Block> SHARC::blocks(StringRef CPU) {
  const Block *Begin = nullptr, *End = nullptr;
  for (const Block &B : Blocks)
    if (B.CPU == CPU) {
      if (!Begin)
        Begin = &B;
      End = &B + 1;
    }
  return Begin ? ArrayRef<Block>(Begin, End) : ArrayRef<Block>();
}

const Block *SHARC::findBlock(StringRef CPU, uint64_t Byte) {
  for (const Block &B : blocks(CPU))
    if (Byte >= B.ByteBase && Byte - B.ByteBase < B.Size)
      return &B;
  return nullptr;
}

std::optional<uint64_t> SHARC::toWord(const Block &B, AddrKind K,
                                      uint64_t Byte) {
  uint64_t Base = B.WordBase[unsigned(K)];
  unsigned Unit = unitBytes(K);
  if (Base == NoView || Byte < B.ByteBase)
    return std::nullopt;
  uint64_t Off = Byte - B.ByteBase;
  if (Off % Unit || Off / Unit >= B.Size / Unit)
    return std::nullopt;
  return Base + Off / Unit;
}

std::optional<uint64_t> SHARC::toByte(StringRef CPU, AddrKind K,
                                      uint64_t Word) {
  unsigned Unit = unitBytes(K);
  for (const Block &B : blocks(CPU)) {
    uint64_t Base = B.WordBase[unsigned(K)];
    if (Base != NoView && Word >= Base && Word - Base < B.Size / Unit)
      return B.ByteBase + (Word - Base) * Unit;
  }
  return std::nullopt;
}

StringRef SHARC::memoryMapCPU(unsigned EFlags) {
  switch (EFlags & ELF::EF_SHARC_MACH) {
  case ELF::EF_SHARC_MACH_2137X:
    switch (EFlags & ELF::EF_SHARC_PART) {
    case ELF::EF_SHARC_PART_21371:
      return "adsp-21371";
    case ELF::EF_SHARC_PART_21375:
      return "adsp-21375";
    default:
      return "";
    }
  case ELF::EF_SHARC_MACH_SHARCPLUS:
    return "adsp-2156x";
  default:
    return "";
  }
}
