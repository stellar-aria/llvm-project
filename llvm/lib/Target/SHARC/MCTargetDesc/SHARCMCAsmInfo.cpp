//===-- SHARCMCAsmInfo.cpp - SHARC assembly syntax ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SHARCMCAsmInfo.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

SHARCMCAsmInfo::SHARCMCAsmInfo(const Triple &TT,
                               const MCTargetOptions &Options)
    : MCAsmInfoELF(Options) {
  // The manuals use C-style comments; '#' is also accepted as a line
  // comment through AllowAdditionalComments. ';' terminates an instruction
  // and is handled by the assembly parser, not as a statement separator.
  CommentString = "//";
  IsLittleEndian = true;
  CodePointerSize = 4;
  CalleeSaveStackSlotSize = 4;
  // Instructions are a whole number of 16-bit parcels.
  MinInstAlignment = 2;
  InternalSymbolPrefix = ".L";
  UsesELFSectionDirectiveForBSS = true;
  // .word is a 32-bit data word (the parser's directive).
  Data32bitsDirective = "\t.word\t";
  SupportsDebugInformation = true;
  // Must agree with Triple::getDefaultExceptionHandling().
  ExceptionsType = ExceptionHandling::DwarfCFI;
}
