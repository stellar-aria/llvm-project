//===-- SHARCELFObjectWriter.cpp - SHARC ELF writer -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SHARCFixupKinds.h"
#include "SHARCMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCValue.h"

using namespace llvm;

namespace {

class SHARCELFObjectWriter : public MCELFObjectTargetWriter {
public:
  SHARCELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64Bit=*/false, OSABI, ELF::EM_SHARC,
                                /*HasRelocationAddend=*/true) {}

protected:
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override {
    switch (unsigned(Fixup.getKind())) {
    case FK_Data_4:
      return ELF::R_SHARC_DATA32;
    case SHARC::fixup_sharc_pcrel24_isa:
      return ELF::R_SHARC_PCREL24_ISA;
    case SHARC::fixup_sharc_pcrel24_visa:
      return ELF::R_SHARC_PCREL24_VISA;
    case SHARC::fixup_sharc_pcrel23_isa:
      return ELF::R_SHARC_PCREL23_ISA;
    case SHARC::fixup_sharc_pcrel23_visa:
      return ELF::R_SHARC_PCREL23_VISA;
    case SHARC::fixup_sharc_pcrel6_isa:
      return ELF::R_SHARC_PCREL6_ISA;
    case SHARC::fixup_sharc_pcrel6_visa:
      return ELF::R_SHARC_PCREL6_VISA;
    case SHARC::fixup_sharc_addr24_isa:
      return ELF::R_SHARC_ADDR24_NW;
    case SHARC::fixup_sharc_addr24_visa:
      return ELF::R_SHARC_ADDR24_SW;
    case SHARC::fixup_sharc_addr32:
      return ELF::R_SHARC_ADDR32_NW;
    default:
      reportError(Fixup.getLoc(), "unsupported relocation");
      return ELF::R_SHARC_NONE;
    }
  }
};

} // end anonymous namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createSHARCELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<SHARCELFObjectWriter>(OSABI);
}
