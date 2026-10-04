//===-- SHARCAsmBackend.cpp - SHARC assembler backend ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SHARCMCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class SHARCAsmBackend : public MCAsmBackend {
  uint8_t OSABI;

public:
  SHARCAsmBackend(uint8_t OSABI)
      : MCAsmBackend(llvm::endianness::little), OSABI(OSABI) {}

  void applyFixup(const MCFragment &, const MCFixup &, const MCValue &,
                  uint8_t *, uint64_t, bool) override {}

  /// Padding is whole NOPs: 48-bit Type 21a, or 16-bit Type 21c parcels.
  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createSHARCELFObjectWriter(OSABI);
  }
};

} // end anonymous namespace

bool SHARCAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                   const MCSubtargetInfo *STI) const {
  if (Count % 2)
    return false;
  bool VISA = STI && STI->hasFeature(SHARC::FeatureVISASection);
  if (!VISA) {
    if (Count % 6)
      return false;
    OS.write_zeros(Count);
    return true;
  }
  // Type 21c: the parcel 0x0001, stored little-endian.
  for (uint64_t I = 0; I < Count / 2; ++I)
    OS.write("\x01\x00", 2);
  return true;
}

MCAsmBackend *llvm::createSHARCAsmBackend(const Target &T,
                                          const MCSubtargetInfo &STI,
                                          const MCRegisterInfo &MRI,
                                          const MCTargetOptions &Options) {
  return new SHARCAsmBackend(
      MCELFObjectTargetWriter::getOSABI(STI.getTargetTriple().getOS()));
}
