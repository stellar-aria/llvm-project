//===-- SHARCInstPrinter.cpp - Print SHARC instructions -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SHARCInstPrinter.h"
#include "SHARCMCTargetDesc.h"
#include "SHARCSyntax.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::SHARC;

#define DEBUG_TYPE "asm-printer"

void SHARCInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  OS << MRI.getName(Reg);
}

void SHARCInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                 StringRef Annot, const MCSubtargetInfo &STI,
                                 raw_ostream &OS) {
  if (!printSyntax(*MI, STI, OS))
    OS << "<unknown encoding>";
  OS << ';';
  printAnnotation(OS, Annot);
}

namespace {

/// Renders one syntax template for one MCInst. See SHARCSyntax.h for the
/// template language.
class Renderer {
  const MCInst &MI;
  const MCSubtargetInfo &STI;
  SHARCInstPrinter &Printer;
  const MCRegisterInfo &MRI;
  const MCAsmInfo &MAI;

public:
  StringSet<> Used;
  bool Absent = false;
  bool Failed = false;

  Renderer(const MCInst &MI, const MCSubtargetInfo &STI,
           SHARCInstPrinter &Printer, const MCRegisterInfo &MRI,
           const MCAsmInfo &MAI)
      : MI(MI), STI(STI), Printer(Printer), MRI(MRI), MAI(MAI) {}

  void render(StringRef T, raw_ostream &OS);
  bool unusedFieldsAreZero() const;

private:
  void markCondsUsed(StringRef Conds);
  void renderPart(StringRef Name, raw_ostream &OS);
  void renderPlaceholder(StringRef Name, ArrayRef<StringRef> Args, char Fmt,
                         raw_ostream &OS);
  void printImm(int64_t V, unsigned Width, char Fmt, raw_ostream &OS);
  void printReg(MCRegister Reg, OpKind Kind, raw_ostream &OS);
  int64_t field(StringRef Name);
};

} // namespace

static size_t matchingClose(StringRef T, size_t Open, char O, char C) {
  unsigned Depth = 0;
  for (size_t I = Open; I < T.size(); ++I) {
    if (T[I] == O)
      ++Depth;
    else if (T[I] == C && --Depth == 0)
      return I;
  }
  llvm_unreachable("unbalanced syntax template");
}

static StringRef takeIdent(StringRef T, size_t &I) {
  size_t B = I;
  while (I < T.size() && (isAlnum(T[I]) || T[I] == '_'))
    ++I;
  return T.slice(B, I);
}

void Renderer::render(StringRef T, raw_ostream &OS) {
  for (size_t I = 0; I < T.size() && !Failed;) {
    char C = T[I];
    if (C == '[') {
      size_t E = matchingClose(T, I, '[', ']');
      bool SavedAbsent = Absent;
      Absent = false;
      SmallString<64> Inner;
      raw_svector_ostream IOS(Inner);
      render(T.slice(I + 1, E), IOS);
      if (!Absent)
        OS << Inner;
      Absent = SavedAbsent;
      I = E + 1;
    } else if (C == '{') {
      size_t E = matchingClose(T, I, '{', '}');
      SmallVector<StringRef, 8> Names;
      T.slice(I + 1, E).split(Names, ',');
      ListSeparator LS;
      for (StringRef N : Names) {
        N = N.trim();
        assert(N.starts_with("%") && "list items must be parts");
        SmallString<32> Item;
        raw_svector_ostream IOS(Item);
        renderPart(N.drop_front(), IOS);
        if (!Item.empty())
          OS << LS << Item;
      }
      I = E + 1;
    } else if (C == '%') {
      ++I;
      renderPart(takeIdent(T, I), OS);
    } else if (C == '$') {
      ++I;
      StringRef Name = takeIdent(T, I);
      SmallVector<StringRef, 3> Args;
      if (I < T.size() && T[I] == '(' && Name.size() <= 3 &&
          isUpper(Name[0])) {
        size_t E = T.find(')', I);
        T.slice(I + 1, E).split(Args, ',');
        for (StringRef &A : Args)
          A = A.trim();
        I = E + 1;
      } else if (Name == "cat") {
        size_t E = T.find(')', I);
        T.slice(I + 1, E).split(Args, ',');
        for (StringRef &A : Args)
          A = A.trim();
        I = E + 1;
      }
      char Fmt = 0;
      if (I + 1 < T.size() && T[I] == ':' && isAlpha(T[I + 1])) {
        Fmt = T[I + 1];
        I += 2;
      }
      renderPlaceholder(Name, Args, Fmt, OS);
    } else {
      OS << C;
      ++I;
    }
  }
}

void Renderer::markCondsUsed(StringRef Conds) {
  SmallVector<StringRef, 8> Terms;
  Conds.split(Terms, ',', -1, false);
  for (StringRef T : Terms)
    if (T.contains('='))
      Used.insert(T.split('=').first.trim());
}

void Renderer::renderPart(StringRef Name, raw_ostream &OS) {
  auto Try = [&](ArrayRef<SyntaxRow> Rows) {
    for (const SyntaxRow &R : Rows) {
      if (Name != R.Part || !condsHold(MI, R.Conds, STI.getFeatureBits()))
        continue;
      markCondsUsed(R.Conds);
      render(R.Template, OS);
      return true;
    }
    return false;
  };
  if (!Try(getSyntaxRows(MI.getOpcode())) && !Try(getCommonParts()))
    Failed = true;
}

int64_t Renderer::field(StringRef Name) {
  Used.insert(Name);
  std::optional<int64_t> V = getField(MI, Name);
  if (!V) {
    Failed = true;
    return 0;
  }
  return *V;
}

void Renderer::printImm(int64_t V, unsigned Width, char Fmt, raw_ostream &OS) {
  switch (Fmt) {
  case 'x':
    OS << "0x" << utohexstr(Width < 64 ? uint64_t(V) & maskTrailingOnes<uint64_t>(Width)
                                       : uint64_t(V),
                            /*LowerCase=*/true);
    return;
  case 's':
    OS << (Width < 64 ? SignExtend64(V, Width) : V);
    return;
  default:
    OS << uint64_t(V);
    return;
  }
}

void Renderer::printReg(MCRegister Reg, OpKind Kind, raw_ostream &OS) {
  switch (Kind) {
  case OpKind::FREG:
  case OpKind::FXM:
  case OpKind::FYM:
  case OpKind::FXA:
  case OpKind::FYA: {
    const MCRegisterClass &RC = MRI.getRegClass(SHARC::RFREGRegClassID);
    for (unsigned I = 0; I < RC.getNumRegs(); ++I)
      if (RC.getRegister(I) == Reg) {
        OS << 'F' << I;
        return;
      }
    break;
  }
  case OpKind::DBL: {
    MCRegister Even = MRI.getSubReg(Reg, SHARC::sub_even);
    const MCRegisterClass &RC = MRI.getRegClass(SHARC::RFREGRegClassID);
    for (unsigned I = 0; I < RC.getNumRegs(); ++I)
      if (RC.getRegister(I) == Even) {
        OS << 'F' << I + 1 << ':' << I;
        return;
      }
    break;
  }
  default:
    OS << MRI.getName(Reg);
    return;
  }
  Failed = true;
}

void Renderer::renderPlaceholder(StringRef Name, ArrayRef<StringRef> Args,
                                 char Fmt, raw_ostream &OS) {
  // Registers derived from a DAG select bit and a 3-bit field.
  if (Name == "I" || Name == "M" || Name == "B") {
    int64_t Index = field(Args[0]) * 8 + field(Args[1]);
    OS << MRI.getName(dagRegister(MRI, Name[0], Index));
    return;
  }
  if (Name == "Ix" || Name == "Bx") {
    int64_t Index = field(Args[0]) * 8 + (field(Args[1]) ^ field(Args[2]));
    OS << MRI.getName(dagRegister(MRI, Name[0], Index));
    return;
  }
  if (Name == "cat") {
    const OperandInfo *Hi = findOperand(MI.getOpcode(), Args[0]);
    const OperandInfo *Lo = findOperand(MI.getOpcode(), Args[1]);
    assert(Hi && Lo && "$cat names unknown fields");
    uint64_t LoV = uint64_t(field(Args[1])) & maskTrailingOnes<uint64_t>(Lo->Width);
    int64_t V = int64_t((uint64_t(field(Args[0])) << Lo->Width) | LoV);
    printImm(V, Hi->Width + Lo->Width, Fmt ? Fmt : 's', OS);
    return;
  }

  const OperandInfo *Info = findOperand(MI.getOpcode(), Name);
  assert(Info && "syntax template names an unknown operand");
  Used.insert(Name);
  const MCOperand &Op = MI.getOperand(Info->Index);

  switch (Info->Kind) {
  case OpKind::Cond: {
    int64_t V = Op.getImm();
    bool Term = Name == "term";
    if (!Term && V == CondTrue) {
      Absent = true;
      return;
    }
    OS << condName(V, Term, STI.getFeatureBits());
    return;
  }
  case OpKind::Compute:
  case OpKind::ShortCompute:
  case OpKind::ShiftImm:
    if (!Op.isInst()) {
      Absent = true;
      return;
    }
    if (!Printer.printSyntax(*Op.getInst(), STI, OS))
      Failed = true;
    return;
  case OpKind::UImm:
  case OpKind::Imm:
  case OpKind::Addr:
  case OpKind::PCRel:
    if (Op.isExpr()) {
      MAI.printExpr(OS, *Op.getExpr());
      return;
    }
    if (!Fmt)
      Fmt = Info->Kind == OpKind::UImm    ? 'd'
            : Info->Kind == OpKind::PCRel ? 's'
                                          : 'x';
    printImm(Op.getImm(), Info->Kind == OpKind::PCRel ? 64 : Info->Width, Fmt,
             OS);
    return;
  default:
    printReg(Op.getReg(), Info->Kind, OS);
    return;
  }
}

bool Renderer::unusedFieldsAreZero() const {
  for (const OperandInfo &O : getOperandInfo(MI.getOpcode())) {
    if (Used.contains(O.Name))
      continue;
    const MCOperand &Op = MI.getOperand(O.Index);
    if (Op.isInst() || (Op.isImm() && Op.getImm() != 0) || Op.isExpr())
      return false;
  }
  return true;
}

bool SHARCInstPrinter::printSyntax(const MCInst &MI, const MCSubtargetInfo &STI,
                                   raw_ostream &OS) {
  for (const SyntaxRow &R : getSyntaxRows(MI.getOpcode())) {
    if (*R.Part || !condsHold(MI, R.Conds, STI.getFeatureBits()))
      continue;
    Renderer Rd(MI, STI, *this, MRI, MAI);
    Rd.Used.clear();
    SmallString<64> Text;
    raw_svector_ostream TOS(Text);
    SmallVector<StringRef, 8> Terms;
    StringRef(R.Conds).split(Terms, ',', -1, false);
    for (StringRef T : Terms)
      if (T.contains('='))
        Rd.Used.insert(T.split('=').first.trim());
    Rd.render(R.Template, TOS);
    if (Rd.Failed || !Rd.unusedFieldsAreZero())
      continue;
    OS << Text;
    return true;
  }
  return false;
}
