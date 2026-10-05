//===- SHARCMemoryMapTest.cpp - SHARC memory block table ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/TargetParser/SHARCMemoryMap.h"
#include "llvm/BinaryFormat/ELF.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::SHARC;

namespace {

uint64_t word(StringRef CPU, AddrKind K, uint64_t Byte) {
  const Block *B = findBlock(CPU, Byte);
  EXPECT_NE(B, nullptr) << "no block at " << Byte;
  if (!B)
    return NoView;
  return toWord(*B, K, Byte).value_or(NoView);
}

// ADSP-2156x data sheet Rev. D, Table 2 (L1) and Table 3 (L2, 8 Mb).
TEST(SHARCMemoryMap, ADSP2156xL1Ends) {
  StringRef C = "adsp-2156x";
  // Block 0: byte 0x00240000-0x0026FFFF.
  EXPECT_EQ(word(C, AddrKind::LW, 0x240000), 0x48000u);
  EXPECT_EQ(word(C, AddrKind::LW, 0x26FFF8), 0x4DFFFu);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x240000), 0x90000u);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x240000 + 6 * 0x7FFF), 0x97FFFu);
  EXPECT_EQ(word(C, AddrKind::NW, 0x26FFFC), 0x9BFFFu);
  EXPECT_EQ(word(C, AddrKind::SWCode, 0x26FFFE), 0x137FFFu);
  EXPECT_EQ(word(C, AddrKind::SWData, 0x240002), 0x120001u);
  EXPECT_EQ(word(C, AddrKind::Byte, 0x26FFFF), 0x26FFFFu);
  // Block 1: byte 0x002C0000-0x002EFFFF.
  EXPECT_EQ(word(C, AddrKind::LW, 0x2EFFF8), 0x5DFFFu);
  EXPECT_EQ(word(C, AddrKind::NW, 0x2EFFFC), 0xBBFFFu);
  EXPECT_EQ(word(C, AddrKind::SWCode, 0x2EFFFE), 0x177FFFu);
  // Block 2: byte 0x00300000-0x0031FFFF; ISA ends at 0xC5554.
  EXPECT_EQ(word(C, AddrKind::LW, 0x31FFF8), 0x63FFFu);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x300000 + 6 * 0x5554), 0xC5554u);
  EXPECT_EQ(word(C, AddrKind::NW, 0x31FFFC), 0xC7FFFu);
  EXPECT_EQ(word(C, AddrKind::SWCode, 0x31FFFE), 0x18FFFFu);
  // Block 3: byte 0x00380000-0x0039FFFF; ISA ends at 0xE5554.
  EXPECT_EQ(word(C, AddrKind::LW, 0x39FFF8), 0x73FFFu);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x380000 + 6 * 0x5554), 0xE5554u);
  EXPECT_EQ(word(C, AddrKind::NW, 0x39FFFC), 0xE7FFFu);
  EXPECT_EQ(word(C, AddrKind::SWCode, 0x39FFFE), 0x1CFFFFu);
}

TEST(SHARCMemoryMap, ADSP2156xL2) {
  StringRef C = "adsp-2156x";
  // L2 RAM 8 Mb: byte 0x20000000-0x200FFFFF, NW 0x08000000-0x0803FFFF,
  // VISA 0x00B80000-0x00BFFFFF, ISA from 0x005C0000.
  EXPECT_EQ(word(C, AddrKind::NW, 0x200FFFFC), 0x0803FFFFu);
  EXPECT_EQ(word(C, AddrKind::SWCode, 0x200FFFFE), 0xBFFFFFu);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x20000000), 0x5C0000u);
  // The table prints the last ISA word as 0x5EAAAA, which would end 2 bytes
  // past L2; whole words end at 0x5EAAA9.
  EXPECT_EQ(word(C, AddrKind::ISA, 0x20000000 + 6 * 0x2AAA9), 0x5EAAA9u);
  const Block *B = findBlock(C, 0x20000000);
  ASSERT_NE(B, nullptr);
  EXPECT_FALSE(toWord(*B, AddrKind::LW, 0x20000000));
  EXPECT_FALSE(toWord(*B, AddrKind::SWData, 0x20000000));
}

// ADSP-21371/21375 data sheet Rev. D, Table 3 (ADSP-21371 RAM). Byte = 2 * SW.
TEST(SHARCMemoryMap, ADSP21371RAMEnds) {
  StringRef C = "adsp-21371";
  // Block 0 RAM: SW 0x130000-0x133FFF, so byte 0x260000-0x267FFF.
  EXPECT_EQ(word(C, AddrKind::LW, 0x260000), 0x4C000u);
  EXPECT_EQ(word(C, AddrKind::LW, 0x267FF8), 0x4CFFFu);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x260000), 0x90000u);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x260000 + 6 * 0x1554), 0x91554u);
  EXPECT_EQ(word(C, AddrKind::NW, 0x260000), 0x98000u);
  EXPECT_EQ(word(C, AddrKind::NW, 0x267FFC), 0x99FFFu);
  EXPECT_EQ(word(C, AddrKind::SWData, 0x267FFE), 0x133FFFu);
  // Block 1 RAM: SW 0x170000-0x173FFF.
  EXPECT_EQ(word(C, AddrKind::LW, 0x2E7FF8), 0x5CFFFu);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x2E0000), 0xB0000u);
  EXPECT_EQ(word(C, AddrKind::NW, 0x2E7FFC), 0xB9FFFu);
  // Block 2 RAM: SW 0x180000-0x183FFF.
  EXPECT_EQ(word(C, AddrKind::LW, 0x307FF8), 0x60FFFu);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x300000 + 6 * 0x1554), 0xC1554u);
  EXPECT_EQ(word(C, AddrKind::NW, 0x307FFC), 0xC1FFFu);
  // Block 3 RAM: SW 0x1C0000-0x1C3FFF.
  EXPECT_EQ(word(C, AddrKind::LW, 0x387FF8), 0x70FFFu);
  EXPECT_EQ(word(C, AddrKind::NW, 0x387FFC), 0xE1FFFu);
  EXPECT_EQ(word(C, AddrKind::SWData, 0x387FFE), 0x1C3FFFu);
  // Classic SHARC has no byte space.
  const Block *B = findBlock(C, 0x260000);
  ASSERT_NE(B, nullptr);
  EXPECT_FALSE(toWord(*B, AddrKind::Byte, 0x260000));
}

// Table 4 (ADSP-21375 RAM): the same bases, half the RAM per block.
TEST(SHARCMemoryMap, ADSP21375RAMEnds) {
  StringRef C = "adsp-21375";
  // Block 0 RAM: SW 0x130000-0x131FFF, so byte 0x260000-0x263FFF.
  EXPECT_EQ(word(C, AddrKind::LW, 0x263FF8), 0x4C7FFu);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x260000 + 6 * 0xAA9), 0x90AA9u);
  EXPECT_EQ(word(C, AddrKind::NW, 0x263FFC), 0x98FFFu);
  EXPECT_EQ(word(C, AddrKind::SWData, 0x263FFE), 0x131FFFu);
  // Block 1 RAM: SW 0x170000-0x171FFF.
  EXPECT_EQ(word(C, AddrKind::LW, 0x2E3FF8), 0x5C7FFu);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x2E0000 + 6 * 0xAA9), 0xB0AA9u);
  EXPECT_EQ(word(C, AddrKind::NW, 0x2E3FFC), 0xB8FFFu);
  EXPECT_EQ(word(C, AddrKind::SWData, 0x2E3FFE), 0x171FFFu);
  // Block 2 RAM: SW 0x180000-0x181FFF.
  EXPECT_EQ(word(C, AddrKind::LW, 0x303FF8), 0x607FFu);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x300000 + 6 * 0xAA9), 0xC0AA9u);
  EXPECT_EQ(word(C, AddrKind::NW, 0x303FFC), 0xC0FFFu);
  EXPECT_EQ(word(C, AddrKind::SWData, 0x303FFE), 0x181FFFu);
  // Block 3 RAM: SW 0x1C0000-0x1C1FFF.
  EXPECT_EQ(word(C, AddrKind::LW, 0x383FF8), 0x707FFu);
  EXPECT_EQ(word(C, AddrKind::ISA, 0x380000 + 6 * 0xAA9), 0xE0AA9u);
  EXPECT_EQ(word(C, AddrKind::NW, 0x383FFC), 0xE0FFFu);
  EXPECT_EQ(word(C, AddrKind::SWData, 0x383FFE), 0x1C1FFFu);
  // Past the smaller RAM.
  EXPECT_EQ(findBlock(C, 0x264000), nullptr);
}

TEST(SHARCMemoryMap, Rejects) {
  StringRef C = "adsp-2156x";
  EXPECT_EQ(findBlock(C, 0x100000), nullptr);             // between blocks
  EXPECT_EQ(findBlock("adsp-214xx", 0x240000), nullptr);  // not in M2
  EXPECT_EQ(findBlock("adsp-2137x", 0x260000), nullptr);  // no part
  const Block *B = findBlock(C, 0x240000);
  ASSERT_NE(B, nullptr);
  EXPECT_FALSE(toWord(*B, AddrKind::NW, 0x240002));  // misaligned
  EXPECT_FALSE(toWord(*B, AddrKind::ISA, 0x240004));
  EXPECT_FALSE(toWord(*B, AddrKind::ISA, 0x26FFFC)); // partial last row
}

TEST(SHARCMemoryMap, ToByteInvertsToWord) {
  for (StringRef C : {"adsp-2156x", "adsp-21371", "adsp-21375"})
    for (const Block &B : blocks(C))
      for (unsigned K = 0; K < 6; ++K) {
        if (B.WordBase[K] == NoView)
          continue;
        auto Kind = AddrKind(K);
        uint64_t Last = B.ByteBase +
                        (B.Size / unitBytes(Kind) - 1) * unitBytes(Kind);
        for (uint64_t Byte : {B.ByteBase, Last}) {
          std::optional<uint64_t> W = toWord(B, Kind, Byte);
          ASSERT_TRUE(W) << B.Name.str() << " kind " << K;
          EXPECT_EQ(toByte(C, Kind, *W), Byte) << B.Name.str() << " kind " << K;
        }
      }
}

TEST(SHARCMemoryMap, EFlags) {
  using namespace ELF;
  EXPECT_EQ(memoryMapCPU(EF_SHARC_MACH_2137X | EF_SHARC_PART_21371),
            "adsp-21371");
  EXPECT_EQ(memoryMapCPU(EF_SHARC_MACH_2137X | EF_SHARC_PART_21375),
            "adsp-21375");
  EXPECT_EQ(memoryMapCPU(EF_SHARC_MACH_2137X), "");
  EXPECT_EQ(memoryMapCPU(EF_SHARC_MACH_SHARCPLUS), "adsp-2156x");
  EXPECT_EQ(memoryMapCPU(EF_SHARC_MACH_214XX), "");
  EXPECT_EQ(memoryMapCPU(EF_SHARC_MACH_NONE), "");
}

} // namespace
