//===- SHARCRelocFieldsTest.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/BinaryFormat/SHARCRelocFields.h"
#include "llvm/BinaryFormat/ELF.h"
#include "gtest/gtest.h"
#include <cstring>

using namespace llvm;
using namespace llvm::SHARC;

namespace {

// Parcels are 16-bit little-endian, most significant parcel first; window
// bit b lives in parcel (47 - b) / 16.

TEST(SHARCRelocFields, Addr24) {
  // Bits 23-0 = 0x90008: parcel 1 (bits 31-16) low byte 0x09, parcel 2
  // (bits 15-0) = 0x0008. Parcel 0 and bits 31-24 are untouched.
  uint8_t I[6] = {0xAA, 0xBB, 0x00, 0xFF, 0x00, 0x00};
  insertField(I, relocFieldSegments(ELF::R_SHARC_ADDR24_NW), 0x90008);
  const uint8_t E[6] = {0xAA, 0xBB, 0x09, 0xFF, 0x08, 0x00};
  EXPECT_EQ(0, memcmp(I, E, 6));
}

TEST(SHARCRelocFields, Addr32) {
  // Bits 31-0 = 0x08000040: parcel 1 = 0x0800, parcel 2 = 0x0040.
  uint8_t I[6] = {0x11, 0x22, 0, 0, 0, 0};
  insertField(I, relocFieldSegments(ELF::R_SHARC_ADDR32_NW), 0x08000040);
  const uint8_t E[6] = {0x11, 0x22, 0x00, 0x08, 0x40, 0x00};
  EXPECT_EQ(0, memcmp(I, E, 6));
}

TEST(SHARCRelocFields, PCRel23KeepsLoopModeBit) {
  // Bit 23 (the SHARC+ loop mode) is outside the 23-bit field.
  uint8_t I[6] = {0, 0, 0x80, 0x00, 0, 0}; // parcel 1 = 0x0080: bit 23 set
  insertField(I, relocFieldSegments(ELF::R_SHARC_PCREL23_ISA), 0x7FFFFF);
  const uint8_t E[6] = {0, 0, 0xFF, 0x00, 0xFF, 0xFF};
  EXPECT_EQ(0, memcmp(I, E, 6));
}

TEST(SHARCRelocFields, PCRel6Split) {
  // -1 = 0b111111: bit 32 gets the top bit, bits 31-27 the rest.
  // Parcel 0 (bits 47-32) = 0x0001, parcel 1 (bits 31-16) = 0xF800.
  uint8_t I[6] = {0, 0, 0, 0, 0x34, 0x12};
  insertField(I, relocFieldSegments(ELF::R_SHARC_PCREL6_ISA), uint64_t(-1));
  const uint8_t E[6] = {0x01, 0x00, 0x00, 0xF8, 0x34, 0x12};
  EXPECT_EQ(0, memcmp(I, E, 6));
}

TEST(SHARCRelocFields, Kinds) {
  EXPECT_TRUE(isPCRelReloc(ELF::R_SHARC_PCREL6_VISA));
  EXPECT_FALSE(isPCRelReloc(ELF::R_SHARC_ADDR24_SW));
  EXPECT_EQ(pcRelRelocUnit(ELF::R_SHARC_PCREL24_ISA), 6u);
  EXPECT_EQ(pcRelRelocUnit(ELF::R_SHARC_PCREL23_VISA), 2u);
  EXPECT_EQ(fieldWidth(relocFieldSegments(ELF::R_SHARC_PCREL6_VISA)), 6u);
  EXPECT_TRUE(relocFieldSegments(ELF::R_SHARC_DATA32).empty());
  EXPECT_TRUE(relocFieldSegments(ELF::R_SHARC_IMM16).empty());
}

} // namespace
