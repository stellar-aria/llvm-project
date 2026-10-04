//===-- SHARCAsmBackend.cpp - SHARC assembler backend ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SHARCFixupKinds.h"
#include "SHARCMCTargetDesc.h"
#include "SHARCParcels.h"
#include "SHARCSyntax.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class SHARCAsmBackend : public MCAsmBackend {
  uint8_t OSABI;

public:
  SHARCAsmBackend(uint8_t OSABI)
      : MCAsmBackend(llvm::endianness::little), OSABI(OSABI) {}

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override;
  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override;

  /// The parser gives an instruction a 48-bit alternative as its last,
  /// extra operand when it may have to grow: near a hardware loop end, or
  /// when a label may be out of range of a 6-bit offset.
  bool mayNeedRelaxation(unsigned Opcode, ArrayRef<MCOperand> Operands,
                         const MCSubtargetInfo &STI) const override {
    return Operands.size() > SHARC::getOperandInfo(Opcode).size() &&
           Operands.back().isInst();
  }
  bool fixupNeedsRelaxationAdvanced(const MCFragment &F, const MCFixup &Fixup,
                                    const MCValue &Target, uint64_t Value,
                                    bool Resolved) const override;
  void relaxInstruction(MCInst &Inst,
                        const MCSubtargetInfo &STI) const override {
    Inst = *Inst.getOperand(Inst.getNumOperands() - 1).getInst();
  }

  /// Padding is whole NOPs: 48-bit Type 21a, or 16-bit Type 21c parcels.
  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createSHARCELFObjectWriter(OSABI);
  }
};

} // end anonymous namespace

MCFixupKindInfo SHARCAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  // Offsets and sizes are not used: applyFixup places the bits itself.
  static const MCFixupKindInfo Infos[SHARC::NumTargetFixupKinds] = {
      {"fixup_sharc_pcrel24_isa", 0, 48, 0},
      {"fixup_sharc_pcrel24_visa", 0, 48, 0},
      {"fixup_sharc_pcrel23_isa", 0, 48, 0},
      {"fixup_sharc_pcrel23_visa", 0, 48, 0},
      {"fixup_sharc_pcrel6_isa", 0, 48, 0},
      {"fixup_sharc_pcrel6_visa", 0, 48, 0},
      {"fixup_sharc_addr24_isa", 0, 48, 0},
      {"fixup_sharc_addr24_visa", 0, 48, 0},
      {"fixup_sharc_addr32", 0, 48, 0},
      {"fixup_sharc_loop_end", 0, 0, 0},
      {"fixup_sharc_loop_end_fixed", 0, 0, 0},
      {"fixup_sharc_loop_mode", 0, 48, 0},
  };
  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);
  return Infos[Kind - FirstTargetFixupKind];
}

void SHARCAsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                                 const MCValue &Target, uint8_t *Data,
                                 uint64_t Value, bool IsResolved) {
  maybeAddReloc(F, Fixup, Target, Value, IsResolved);
  unsigned Kind = Fixup.getKind();
  if (Kind < FirstTargetFixupKind) {
    // Plain data (.word): little-endian.
    unsigned Size = getFixupKindInfo(Kind).TargetSize / 8;
    for (unsigned I = 0; I < Size; ++I)
      Data[I] |= uint8_t(Value >> (8 * I));
    return;
  }
  if (Kind == SHARC::fixup_sharc_loop_end ||
      Kind == SHARC::fixup_sharc_loop_end_fixed ||
      Kind == SHARC::fixup_sharc_loop_mode) {
    if (!IsResolved) {
      getContext().reportError(Fixup.getLoc(), "hardware loop is not closed");
      return;
    }
    if (Kind == SHARC::fixup_sharc_loop_end_fixed) {
      bool Plus = F.getSubtargetInfo()->hasFeature(SHARC::FeatureSHARCPlus);
      if (int64_t(Value) >= 1 &&
          int64_t(Value) <= SHARC::loopEndInstructions(Plus))
        getContext().reportError(Fixup.getLoc(),
                                 "no 48-bit form of this instruction for the "
                                 "end of a hardware loop");
      return;
    }
    if (Kind == SHARC::fixup_sharc_loop_end)
      return;
  }
  // With a relocation the field stays zero; the addend carries the offset.
  if (!IsResolved)
    return;

  int64_t V = int64_t(Value);
  unsigned Width = 0;
  for (auto [Hi, Lo] : SHARC::fixupSegments(Kind))
    Width += Hi - Lo + 1;
  if (SHARC::isPCRelFixup(Kind)) {
    unsigned Unit = SHARC::pcRelUnit(Kind);
    if (V % Unit) {
      getContext().reportError(Fixup.getLoc(),
                               "branch target is not on an instruction "
                               "boundary");
      return;
    }
    V /= Unit;
    if (!isIntN(Width, V)) {
      getContext().reportError(Fixup.getLoc(),
                               "branch target out of range of a " +
                                   Twine(Width) + "-bit offset");
      return;
    }
  } else if (!isUIntN(Width, V) && !isIntN(Width, V)) {
    getContext().reportError(Fixup.getLoc(), "value out of range of a " +
                                                 Twine(Width) + "-bit field");
    return;
  }

  // Scatter the value over the field's bits, most significant segment first,
  // and rewrite the parcels the field touches.
  unsigned Avail;
  ArrayRef<uint8_t> Bytes(Data, 6);
  uint64_t W = SHARC::readWindow(Bytes, Avail);
  unsigned Remaining = Width;
  unsigned FirstParcel = 2, LastParcel = 0;
  for (auto [Hi, Lo] : SHARC::fixupSegments(Kind)) {
    unsigned N = Hi - Lo + 1;
    Remaining -= N;
    uint64_t Part = (uint64_t(V) >> Remaining) & maskTrailingOnes<uint64_t>(N);
    W = (W & ~(maskTrailingOnes<uint64_t>(N) << Lo)) | (Part << Lo);
    FirstParcel = std::min(FirstParcel, (47 - Hi) / 16);
    LastParcel = std::max(LastParcel, (47 - Lo) / 16);
  }
  for (unsigned P = FirstParcel; P <= LastParcel; ++P) {
    uint16_t Parcel = (W >> (32 - 16 * P)) & 0xFFFF;
    Data[2 * P] = uint8_t(Parcel);
    Data[2 * P + 1] = uint8_t(Parcel >> 8);
  }
}

bool SHARCAsmBackend::fixupNeedsRelaxationAdvanced(const MCFragment &F,
                                                   const MCFixup &Fixup,
                                                   const MCValue &Target,
                                                   uint64_t Value,
                                                   bool Resolved) const {
  unsigned Kind = Fixup.getKind();
  if (Kind == SHARC::fixup_sharc_loop_end) {
    bool Plus = F.getSubtargetInfo()->hasFeature(SHARC::FeatureSHARCPlus);
    return Resolved && int64_t(Value) >= 1 &&
           int64_t(Value) <= SHARC::loopEndInstructions(Plus);
  }
  if (Kind == SHARC::fixup_sharc_pcrel6_visa ||
      Kind == SHARC::fixup_sharc_pcrel6_isa) {
    unsigned Unit = SHARC::pcRelUnit(Kind);
    int64_t V = int64_t(Value);
    return !Resolved || V % Unit || !isInt<6>(V / Unit);
  }
  return false;
}

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
