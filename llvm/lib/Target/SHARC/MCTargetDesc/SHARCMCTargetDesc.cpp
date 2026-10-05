//===-- SHARCMCTargetDesc.cpp - SHARC target descriptions -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SHARCMCTargetDesc.h"
#include "SHARCInstPrinter.h"
#include "SHARCMCAsmInfo.h"
#include "SHARCTargetStreamer.h"
#include "TargetInfo/SHARCTargetInfo.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/SHARCMemoryMap.h"

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "SHARCGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "SHARCGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "SHARCGenSubtargetInfo.inc"

using namespace llvm;

static MCAsmInfo *createSHARCMCAsmInfo(const MCRegisterInfo &MRI,
                                       const Triple &TT,
                                       const MCTargetOptions &Options) {
  return new SHARCMCAsmInfo(TT, Options);
}

namespace {
/// Code sections are aligned to the 16-bit parcel. 48-bit code is 6 bytes per
/// instruction, so an alignment that does not divide 6 would move a section
/// concatenated after another off its 48-bit rows.
class SHARCMCObjectFileInfo : public MCObjectFileInfo {
public:
  unsigned getTextSectionAlignment() const override { return 2; }
};
} // namespace

static MCObjectFileInfo *createSHARCMCObjectFileInfo(MCContext &Ctx, bool PIC,
                                                     bool LargeCodeModel) {
  MCObjectFileInfo *MOFI = new SHARCMCObjectFileInfo();
  MOFI->initMCObjectFileInfo(Ctx, PIC, LargeCodeModel);
  return MOFI;
}

namespace {
/// Absolute addresses in instruction fields are word addresses; map them back
/// to ELF byte addresses through the processor's block table so that
/// llvm-objdump can name their symbols.
class SHARCMCInstrAnalysis : public MCInstrAnalysis {
public:
  explicit SHARCMCInstrAnalysis(const MCInstrInfo *Info)
      : MCInstrAnalysis(Info) {}

  std::optional<uint64_t>
  evaluateMemoryOperandAddress(const MCInst &Inst, const MCSubtargetInfo *STI,
                               uint64_t Addr, uint64_t Size) const override {
    if (!STI)
      return std::nullopt;
    SHARC::AddrKind Kind;
    switch (Inst.getOpcode()) {
    case SHARC::T8a_abs:
    case SHARC::T25a_direct:
      // A code address: 48-bit in ISA code, short-word in VISA code.
      Kind = STI->hasFeature(SHARC::FeatureVISASection)
                 ? SHARC::AddrKind::SWCode
                 : SHARC::AddrKind::ISA;
      break;
    case SHARC::T14a:
      // Operand 2 is l, which selects a long-word access (PGR Type 14a).
      Kind = Inst.getOperand(2).getImm() ? SHARC::AddrKind::LW
                                         : SHARC::AddrKind::NW;
      break;
    default:
      return std::nullopt;
    }
    // The address is the last operand of each of these forms.
    const MCOperand &Op = Inst.getOperand(Inst.getNumOperands() - 1);
    if (!Op.isImm())
      return std::nullopt;
    return SHARC::toByte(STI->getCPU(), Kind, uint64_t(Op.getImm()));
  }
};
} // namespace

static MCInstrAnalysis *createSHARCMCInstrAnalysis(const MCInstrInfo *Info) {
  return new SHARCMCInstrAnalysis(Info);
}

static MCInstrInfo *createSHARCMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitSHARCMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createSHARCMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitSHARCMCRegisterInfo(X, SHARC::PC);
  return X;
}

static MCSubtargetInfo *
createSHARCMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  if (CPU.empty())
    CPU = "adsp-2137x";
  return createSHARCMCSubtargetInfoImpl(TT, CPU, /*TuneCPU=*/CPU, FS);
}

static MCInstPrinter *createSHARCMCInstPrinter(const Triple &T,
                                               unsigned SyntaxVariant,
                                               const MCAsmInfo &MAI,
                                               const MCInstrInfo &MII,
                                               const MCRegisterInfo &MRI) {
  return new SHARCInstPrinter(MAI, MII, MRI);
}

static MCTargetStreamer *
createSHARCAsmTargetStreamer(MCStreamer &S, formatted_raw_ostream &OS,
                             MCInstPrinter *InstPrint) {
  return new SHARCTargetAsmStreamer(S, OS);
}

static MCTargetStreamer *
createSHARCObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
  return new SHARCTargetELFStreamer(S, STI);
}

static MCTargetStreamer *createSHARCNullTargetStreamer(MCStreamer &S) {
  return new SHARCTargetStreamer(S);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeSHARCTargetMC() {
  Target &T = getTheSHARCTarget();
  TargetRegistry::RegisterMCAsmInfo(T, createSHARCMCAsmInfo);
  TargetRegistry::RegisterMCObjectFileInfo(T, createSHARCMCObjectFileInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createSHARCMCInstrInfo);
  TargetRegistry::RegisterMCInstrAnalysis(T, createSHARCMCInstrAnalysis);
  TargetRegistry::RegisterMCRegInfo(T, createSHARCMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createSHARCMCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createSHARCMCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(T, createSHARCMCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createSHARCAsmBackend);
  TargetRegistry::RegisterAsmTargetStreamer(T, createSHARCAsmTargetStreamer);
  TargetRegistry::RegisterObjectTargetStreamer(T,
                                               createSHARCObjectTargetStreamer);
  TargetRegistry::RegisterNullTargetStreamer(T, createSHARCNullTargetStreamer);
}
