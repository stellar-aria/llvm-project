//===-- SHARCSyntax.cpp - SHARC instruction syntax tables -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SHARCSyntax.h"
#include "SHARCMCTargetDesc.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/TargetParser/SubtargetFeature.h"
#include <vector>

using namespace llvm;
using namespace llvm::SHARC;

namespace {

struct OperandRow {
  unsigned Opcode;
  OperandInfo Info;
};

const OperandRow OperandRows[] = {
#define SHARC_OPERAND(INST, NAME, INDEX, KIND, WIDTH)                          \
  {SHARC::INST, {#NAME, INDEX, OpKind::KIND, WIDTH}},
#include "SHARCOperandInfo.def"
#undef SHARC_OPERAND
};

struct FormSyntaxRow {
  unsigned Opcode; // 0 for a SHARC_COMMON_PART row.
  SyntaxRow Row;
};

const FormSyntaxRow FormSyntaxRows[] = {
#define SHARC_SYNTAX(FORM, CONDS, TEMPLATE) {SHARC::FORM, {"", CONDS, TEMPLATE}},
#define SHARC_PART(FORM, PART, CONDS, TEMPLATE)                                \
  {SHARC::FORM, {#PART, CONDS, TEMPLATE}},
#define SHARC_COMMON_PART(PART, CONDS, TEMPLATE) {0, {#PART, CONDS, TEMPLATE}},
#include "SHARCFormSyntax.def"
};

struct ComputeSyntaxRow {
  unsigned Opcode;
  SyntaxRow Row;
};

const ComputeSyntaxRow ComputeRows[] = {
#define SHARC_COMPUTE(OPC, SYNTAX) {SHARC::OPC, {"", "", SYNTAX}},
#include "SHARCComputeSyntax.def"
#undef SHARC_COMPUTE
};

struct Tables {
  std::vector<ArrayRef<OperandInfo>> Operands;
  std::vector<std::vector<SyntaxRow>> Syntax;
  std::vector<SyntaxRow> Common;
  std::vector<bool> IsComputation;
  std::vector<OperandInfo> OperandStorage;

  Tables() {
    Operands.resize(SHARC::INSTRUCTION_LIST_END);
    Syntax.resize(SHARC::INSTRUCTION_LIST_END);
    IsComputation.resize(SHARC::INSTRUCTION_LIST_END);
    // OperandRows are grouped by opcode in the .def.
    OperandStorage.reserve(std::size(OperandRows));
    for (const OperandRow &R : OperandRows)
      OperandStorage.push_back(R.Info);
    for (size_t I = 0; I < std::size(OperandRows);) {
      size_t J = I;
      while (J < std::size(OperandRows) &&
             OperandRows[J].Opcode == OperandRows[I].Opcode)
        ++J;
      Operands[OperandRows[I].Opcode] =
          ArrayRef<OperandInfo>(OperandStorage.data() + I, J - I);
      I = J;
    }
    for (const FormSyntaxRow &R : FormSyntaxRows) {
      if (R.Opcode == 0)
        Common.push_back(R.Row);
      else
        Syntax[R.Opcode].push_back(R.Row);
    }
    for (const ComputeSyntaxRow &R : ComputeRows) {
      Syntax[R.Opcode].push_back(R.Row);
      IsComputation[R.Opcode] = true;
    }
  }
};

const Tables &tables() {
  static const Tables T;
  return T;
}

} // namespace

ArrayRef<OperandInfo> SHARC::getOperandInfo(unsigned Opcode) {
  return tables().Operands[Opcode];
}

const OperandInfo *SHARC::findOperand(unsigned Opcode, StringRef Name) {
  for (const OperandInfo &O : getOperandInfo(Opcode))
    if (Name == O.Name)
      return &O;
  return nullptr;
}

ArrayRef<SyntaxRow> SHARC::getSyntaxRows(unsigned Opcode) {
  return tables().Syntax[Opcode];
}

ArrayRef<SyntaxRow> SHARC::getCommonParts() { return tables().Common; }

bool SHARC::isComputation(unsigned Opcode) {
  return tables().IsComputation[Opcode];
}

std::optional<int64_t> SHARC::getField(const MCInst &MI, StringRef Name) {
  const OperandInfo *O = findOperand(MI.getOpcode(), Name);
  if (!O || O->Index >= MI.getNumOperands())
    return std::nullopt;
  const MCOperand &Op = MI.getOperand(O->Index);
  if (!Op.isImm())
    return std::nullopt;
  return Op.getImm();
}

static std::optional<unsigned> featureByName(StringRef Name) {
  return StringSwitch<std::optional<unsigned>>(Name)
      .Case("sharcplus", SHARC::FeatureSHARCPlus)
      .Case("visa", SHARC::FeatureVISA)
      .Case("byte-addr", SHARC::FeatureByteAddr)
      .Case("fp64", SHARC::FeatureFP64)
      .Case("visa-section", SHARC::FeatureVISASection)
      .Default(std::nullopt);
}

bool SHARC::condsHold(const MCInst &MI, StringRef Conds,
                      const FeatureBitset &FB) {
  SmallVector<StringRef, 8> Terms;
  Conds.split(Terms, ',', -1, /*KeepEmpty=*/false);
  for (StringRef T : Terms) {
    T = T.trim();
    // Rows for parsing only (e.g. "DO label UNTIL LCE") never print.
    if (T == "@parse")
      return false;
    if (T.consume_front("+") || T.starts_with("-")) {
      bool Want = !T.consume_front("-");
      std::optional<unsigned> F = featureByName(T);
      assert(F && "unknown feature in syntax conditions");
      if (FB[*F] != Want)
        return false;
      continue;
    }
    auto [Name, Value] = T.split('=');
    int64_t V;
    if (Value.getAsInteger(0, V))
      llvm_unreachable("malformed syntax condition");
    // A field still symbolic (the SHARC+ loop mode before its loop closes)
    // prints as its default, 0.
    const OperandInfo *O = findOperand(MI.getOpcode(), Name);
    if (O && MI.getOperand(O->Index).isExpr()) {
      if (V != 0)
        return false;
      continue;
    }
    std::optional<int64_t> F = getField(MI, Name);
    assert(findOperand(MI.getOpcode(), Name) &&
           "syntax condition names an unknown field");
    if (!F || *F != V)
      return false;
  }
  return true;
}

static const char *const CondNames[32] = {
    "EQ",     "LT",     "LE",     "AC",         "AV",         "MV",
    "MS",     "SV",     "SZ",     "FLAG0_IN",   "FLAG1_IN",   "FLAG2_IN",
    "FLAG3_IN", "TF",   "SF",     "NOT LCE",    "NE",         "GE",
    "GT",     "NOT AC", "NOT AV", "NOT MV",     "NOT MS",     "NOT SV",
    "NOT SZ", "NOT FLAG0_IN", "NOT FLAG1_IN", "NOT FLAG2_IN",
    "NOT FLAG3_IN", "NOT TF", "NOT SF", "TRUE"};

StringRef SHARC::condName(unsigned Code, bool Term, const FeatureBitset &FB) {
  assert(Code < 32 && "condition codes are 5 bits");
  if (Term && Code == 15)
    return "LCE";
  if (Term && Code == 31)
    return "FOREVER";
  // Bit 14: bus master on the ADSP-2136x/2137x, shifter bit FIFO from the
  // ADSP-214xx on (PGR Table 10-4 note 1).
  if (!FB[SHARC::FeatureVISA] && (Code == 14 || Code == 30))
    return Code == 14 ? "BM" : "NOT BM";
  return CondNames[Code];
}

std::optional<unsigned> SHARC::parseCondName(StringRef Name, bool Term) {
  std::string Upper = Name.upper();
  StringRef N(Upper);
  if (Term && N == "LCE")
    return 15;
  if (Term && N == "FOREVER")
    return 31;
  if (N == "BM")
    return 14;
  if (N == "NOT BM")
    return 30;
  for (unsigned I = 0; I < 32; ++I) {
    if (N == CondNames[I]) {
      // Only DO UNTIL uses LCE; only IF uses NOT LCE and TRUE.
      if (Term && (I == 15 || I == 31))
        return std::nullopt;
      return I;
    }
  }
  return std::nullopt;
}

MCRegister SHARC::dagRegister(const MCRegisterInfo &MRI, char Kind,
                              unsigned Index) {
  unsigned RC;
  switch (Kind) {
  case 'I':
    RC = SHARC::IREGRegClassID;
    break;
  case 'M':
    RC = SHARC::MREGRegClassID;
    break;
  case 'B':
    RC = SHARC::BREGRegClassID;
    break;
  default:
    llvm_unreachable("not a DAG register kind");
  }
  return MRI.getRegClass(RC).getRegister(Index);
}
