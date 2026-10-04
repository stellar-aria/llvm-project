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
#include "TargetInfo/SHARCTargetInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

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

extern "C" LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeSHARCTargetMC() {
  Target &T = getTheSHARCTarget();
  TargetRegistry::RegisterMCAsmInfo(T, createSHARCMCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createSHARCMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createSHARCMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createSHARCMCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createSHARCMCInstPrinter);
}
