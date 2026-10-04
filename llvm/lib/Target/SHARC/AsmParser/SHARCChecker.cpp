//===-- SHARCChecker.cpp - Diagnose SHARC sequencing hazards --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SHARCChecker.h"
#include "MCTargetDesc/SHARCMCTargetDesc.h"
#include "MCTargetDesc/SHARCSyntax.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;
using namespace llvm::SHARC;

static cl::opt<bool> PredictorWarnings(
    "sharc-predictor-warnings",
    cl::desc("Warn about code the SHARC+ branch predictor guidance advises "
             "against (PRM, Restrictions Related to the Branch Predictor)"),
    cl::init(false));

namespace {

/// What an instruction does, as far as the checks care.
struct Facts {
  bool JumpOrCall = false; // Types 8, 9, 10, 25 (CJUMP)
  bool Return = false;     // Type 11
  bool Delayed = false;    // (DB)
  bool Idle = false;       // Type 22
  bool PCStackPushPop = false;
  std::optional<unsigned> Cond;
  MCRegister Written;       // Ureg written by a move or load
  bool WrittenFromMemory = false;
};

Facts factsOf(const MCInst &I, const MCInstrInfo &MII) {
  Facts F;
  StringRef Name = MII.getName(I.getOpcode());
  auto Field = [&](StringRef N) { return getField(I, N); };
  auto Reg = [&](StringRef N) -> MCRegister {
    const OperandInfo *O = findOperand(I.getOpcode(), N);
    if (!O || !I.getOperand(O->Index).isReg())
      return MCRegister();
    return I.getOperand(O->Index).getReg();
  };
  F.JumpOrCall = Name.starts_with("T8a") || Name.starts_with("T9") ||
                 Name.starts_with("T10a") || Name == "T25a_direct" ||
                 Name == "T25a_pcrel";
  F.Return = Name.starts_with("T11");
  F.Delayed = Name.starts_with("T25a_") ? Name != "T25a_rframe"
                                        : Field("j").value_or(0) == 1;
  F.Idle = Name.starts_with("T22");
  F.PCStackPushPop =
      Name == "T20a" && (Field("ppu").value_or(0) || Field("ppo").value_or(0));
  if (std::optional<int64_t> C = Field("cond"))
    F.Cond = *C;
  if (Name.starts_with("T5a_move") || Name.starts_with("T5b_move")) {
    F.Written = Reg("dstureg");
  } else if (Name.starts_with("T17")) {
    F.Written = Reg("ureg");
  } else if (Field("d").value_or(1) == 0 && Reg("ureg")) {
    F.Written = Reg("ureg");
    F.WrittenFromMemory = true;
  }
  return F;
}

bool mutuallyExclusive(std::optional<unsigned> A, std::optional<unsigned> B) {
  return A && B && *A != CondTrue && *B != CondTrue && (*A ^ *B) == 16;
}

} // namespace

void SHARCChecker::check(const MCInst &I, SMLoc Loc,
                         const MCSubtargetInfo &STI, const MCSection *Sec) {
  Facts F = factsOf(I, MII);
  StringRef Name = MII.getName(I.getOpcode());

  // Delay slots (PGR 4-23).
  if (SlotsLeft) {
    --SlotsLeft;
    Facts B = factsOf(*DelayedBranch, MII);
    if ((F.JumpOrCall || F.Return) && !mutuallyExclusive(B.Cond, F.Cond))
      Parser.Error(Loc, "branch in the delay slot of a delayed branch");
    if (F.PCStackPushPop)
      Parser.Error(Loc,
                   "PC stack push or pop in the delay slot of a delayed branch");
    if (F.Written == SHARC::PCSTK || F.Written == SHARC::PCSTKP)
      Parser.Error(Loc, "write to PCSTK or PCSTKP in the delay slot of a "
                        "delayed branch");
    if (F.Idle)
      Parser.Error(Loc, "IDLE in the delay slot of a delayed branch");
  }
  if ((F.JumpOrCall || F.Return) && F.Delayed && !SlotsLeft) {
    DelayedBranch = &I;
    SlotsLeft = 2;
  }

  // IF NOT LCE after a write to CURLCNTR (PGR 4-55).
  if (PrevWritesCurlcntr && F.Cond == 15u)
    Parser.Error(Loc, "IF NOT LCE after a write to CURLCNTR");
  PrevWritesCurlcntr = F.Written == SHARC::CURLCNTR;

  // A dual move loading one register on both buses (PRM 7-28).
  if ((Name == "T1a" || Name == "T1b") && getField(I, "dmd") == 0 &&
      getField(I, "pmd") == 0) {
    const MCOperand &DM =
        I.getOperand(findOperand(I.getOpcode(), "dmdreg")->Index);
    const MCOperand &PM =
        I.getOperand(findOperand(I.getOpcode(), "pmdreg")->Index);
    if (DM.getReg() == PM.getReg())
      Parser.Error(Loc, Twine("both memory moves load ") +
                            Parser.getContext().getRegisterInfo()->getName(
                                DM.getReg()));
  }

  // SHARC+ branch predictor guidance (PRM 4-12).
  if (!PredictorWarnings || !STI.hasFeature(SHARC::FeatureSHARCPlus))
    return;
  unsigned Size = MII.get(I.getOpcode()).getSize();
  bool Branch = F.JumpOrCall || F.Return;
  auto &PB = Pending[Sec];
  for (PendingBranch &P : PB)
    P.BytesAfter += Size;
  erase_if(PB, [](const PendingBranch &P) { return P.BytesAfter >= 72; });
  // In VISA code, two branches may not share a 48-bit fetch window: no more
  // than one parcel may separate them.
  if (Branch && STI.hasFeature(SHARC::FeatureVISASection)) {
    auto It = SinceBranch.find(Sec);
    if (It != SinceBranch.end() && It->second <= 2)
      Parser.Warning(Loc, "two branches in one 48-bit fetch window");
  }
  if (SinceBranch.count(Sec))
    SinceBranch[Sec] += Size;
  if (Branch) {
    PB.push_back({Loc, 0});
    SinceBranch[Sec] = 0;
  }
}

void SHARCChecker::checkLoop(ArrayRef<LoopInst> Body, bool CounterLoop) {
  unsigned T = Body.size();
  for (unsigned K = 0; K < T; ++K) {
    Facts F = factsOf(*Body[K].Inst, MII);
    unsigned FromEnd = T - K; // 1 for the last instruction
    if (F.JumpOrCall && FromEnd <= 3)
      Parser.Error(Body[K].Loc, "JUMP or CALL in the last three instructions "
                                "of a hardware loop");
    if (F.Idle) {
      bool Bad = CounterLoop ? (T <= 3 || (T == 4 && FromEnd == 1) ||
                                (T > 4 && FromEnd == 5))
                             : FromEnd <= 3;
      if (Bad)
        Parser.Error(Body[K].Loc,
                     "IDLE or EMUIDLE at this position of a hardware loop");
    }
    if (CounterLoop && FromEnd == 5 && F.Written == SHARC::CURLCNTR &&
        F.WrittenFromMemory)
      Parser.Error(Body[K].Loc, "load of CURLCNTR at the fifth-from-last "
                                "instruction of a counter loop");
  }
}

void SHARCChecker::finish() {
  for (auto &[Sec, PB] : Pending)
    for (const PendingBranch &P : PB)
      Parser.Warning(P.Loc, "fewer than twelve 48-bit instructions follow "
                            "this branch");
  Pending.clear();
}
