//===-- SHARCChecker.h - Diagnose SHARC sequencing hazards ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Instruction sequences the manuals say tools should reject: operations in
// the two delay slots of a delayed branch (PGR 4-23), instructions near the
// end of a hardware loop (PGR 4-55), IF NOT LCE after a write to CURLCNTR,
// and dual loads of one register (PRM 7-28). With
// -sharc-predictor-warnings, also the SHARC+ branch predictor guidance
// (PRM 4-12).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SHARC_ASMPARSER_SHARCCHECKER_H
#define LLVM_LIB_TARGET_SHARC_ASMPARSER_SHARCCHECKER_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/SMLoc.h"

namespace llvm {
class MCAsmParser;
class MCInst;
class MCInstrInfo;
class MCSection;
class MCSubtargetInfo;

class SHARCChecker {
public:
  struct LoopInst {
    SMLoc Loc;
    const MCInst *Inst;
  };

  SHARCChecker(MCAsmParser &Parser, const MCInstrInfo &MII)
      : Parser(Parser), MII(MII) {}

  /// Check one instruction in program order.
  void check(const MCInst &I, SMLoc Loc, const MCSubtargetInfo &STI,
             const MCSection *Sec);
  /// Check a hardware loop when its last instruction is known.
  void checkLoop(ArrayRef<LoopInst> Body, bool CounterLoop);
  /// End of input: branches without enough code after them.
  void finish();

private:
  MCAsmParser &Parser;
  const MCInstrInfo &MII;

  // Delayed branch whose slots are being checked.
  const MCInst *DelayedBranch = nullptr;
  unsigned SlotsLeft = 0;
  bool PrevWritesCurlcntr = false;

  // Predictor guidance: per section, the branches with fewer than twelve
  // 48-bit instructions of code after them, and the bytes since the last
  // branch.
  struct PendingBranch {
    SMLoc Loc;
    unsigned BytesAfter;
  };
  DenseMap<const MCSection *, SmallVector<PendingBranch, 2>> Pending;
  DenseMap<const MCSection *, unsigned> SinceBranch;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_SHARC_ASMPARSER_SHARCCHECKER_H
