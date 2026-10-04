//===-- SHARCInstPrinter.h - Print SHARC instructions -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCINSTPRINTER_H
#define LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCINSTPRINTER_H

#include "llvm/MC/MCInstPrinter.h"

namespace llvm {

/// Prints SHARC's algebraic syntax from the shared syntax tables
/// (SHARCSyntax.h). There are no mnemonics.
class SHARCInstPrinter : public MCInstPrinter {
public:
  SHARCInstPrinter(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                   const MCRegisterInfo &MRI)
      : MCInstPrinter(MAI, MII, MRI) {}

  std::pair<const char *, uint64_t>
  getMnemonic(const MCInst &MI) const override {
    return {nullptr, 0};
  }
  void printRegName(raw_ostream &OS, MCRegister Reg) override;
  void printInst(const MCInst *MI, uint64_t Address, StringRef Annot,
                 const MCSubtargetInfo &STI, raw_ostream &OS) override;

  /// Print a form or computation without the trailing ';'. Returns false
  /// if no syntax row accounts for the encoding.
  bool printSyntax(const MCInst &MI, const MCSubtargetInfo &STI,
                   raw_ostream &OS);
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCINSTPRINTER_H
