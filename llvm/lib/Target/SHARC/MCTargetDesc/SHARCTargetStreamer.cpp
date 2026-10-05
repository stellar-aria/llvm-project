//===-- SHARCTargetStreamer.cpp - SHARC target streamer -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SHARCTargetStreamer.h"
#include "SHARCMCTargetDesc.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

void SHARCTargetAsmStreamer::emitDirectiveISA() { OS << "\t.isa\n"; }
void SHARCTargetAsmStreamer::emitDirectiveVISA() { OS << "\t.visa\n"; }

SHARCTargetELFStreamer::SHARCTargetELFStreamer(MCStreamer &S,
                                               const MCSubtargetInfo &STI)
    : SHARCTargetStreamer(S) {
  unsigned Mach =
      STI.hasFeature(SHARC::FeatureSHARCPlus)
          ? ELF::EF_SHARC_MACH_SHARCPLUS
          : StringSwitch<unsigned>(STI.getCPU())
                .Case("adsp-2136x", ELF::EF_SHARC_MACH_2136X)
                .Case("adsp-214xx", ELF::EF_SHARC_MACH_214XX)
                .Default(ELF::EF_SHARC_MACH_2137X);
  if (STI.getCPU().starts_with("adsp-2137"))
    Mach = ELF::EF_SHARC_MACH_2137X;
  unsigned Part = StringSwitch<unsigned>(STI.getCPU())
                      .Case("adsp-21371", ELF::EF_SHARC_PART_21371)
                      .Case("adsp-21375", ELF::EF_SHARC_PART_21375)
                      .Default(ELF::EF_SHARC_PART_NONE);
  ELFObjectWriter &W = static_cast<MCELFStreamer &>(S).getWriter();
  W.setELFHeaderEFlags(
      (W.getELFHeaderEFlags() & ~(ELF::EF_SHARC_MACH | ELF::EF_SHARC_PART)) |
      Mach | Part);
}
