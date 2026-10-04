//===-- SHARCAsmParser.cpp - Parse SHARC assembly ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// SHARC assembly is algebraic: "IF EQ R0 = R1 + R2, DM(I0, M1) = R5;". There
// are no mnemonics, so instead of the generated matcher, each statement is
// matched against the syntax templates the printer uses (SHARCSyntax.h).
// Matching backtracks over forms, optional groups and fragments; a
// fragment's conditions ("u=1,d=0") become field values. Of the forms that
// match the whole statement, the smallest one valid in the current section
// is chosen.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/SHARCMCTargetDesc.h"
#include "MCTargetDesc/SHARCSyntax.h"
#include "MCTargetDesc/SHARCTargetStreamer.h"
#include "TargetInfo/SHARCTargetInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/StringSaver.h"
#include <memory>

using namespace llvm;
using namespace llvm::SHARC;

#define GET_COMPUTE_FEATURES
#include "SHARCGenInstrInfo.inc"

#define DEBUG_TYPE "sharc-asm-parser"

namespace {

//===----------------------------------------------------------------------===//
// Templates
//===----------------------------------------------------------------------===//

/// A parsed syntax template (see SHARCSyntax.h).
struct Node {
  enum KindTy { Literal, Placeholder, Part, Optional, List } Kind;
  AsmToken Tok;                  // Literal
  std::string Name;              // Placeholder, Part
  char Fmt = 0;                  // Placeholder print format; 'L' = symbol
  SmallVector<std::string, 3> Args;
  std::vector<Node> Children;    // Optional; List (Part nodes)
};
using Seq = std::vector<Node>;

//===----------------------------------------------------------------------===//
// Matching state
//===----------------------------------------------------------------------===//

struct Value {
  enum KindTy { Imm, Expr, Reg, Inst } Kind = Imm;
  int64_t I = 0;
  const MCExpr *E = nullptr;
  MCRegister R;
  MCInst *In = nullptr;

  bool operator==(const Value &O) const {
    return Kind == O.Kind && I == O.I && E == O.E && R == O.R && In == O.In;
  }
};

/// Destination register written as Is xor field: resolved once Is is known.
struct XorDest {
  std::string G, A, B;
  unsigned Index;
};

struct State {
  SmallVector<std::pair<std::string, Value>, 12> Fields;
  SmallVector<XorDest, 1> Deferred;
  /// Template keywords read as symbols ("MR0F" in "R3 = MR0F"): a parse
  /// that needs fewer wins.
  unsigned KeywordSymbols = 0;
  /// Symbols in 6-bit PC-relative fields: a parse with fewer wins among
  /// forms of one size, since a wider form needs no relaxation.
  unsigned NarrowSymbols = 0;

  const Value *get(StringRef N) const {
    for (const auto &F : Fields)
      if (F.first == N)
        return &F.second;
    return nullptr;
  }
  /// Set a field; false if it already holds a different value.
  bool set(StringRef N, const Value &V) {
    if (const Value *Old = get(N))
      return *Old == V;
    Fields.push_back({N.str(), V});
    return true;
  }
  bool setImm(StringRef N, int64_t I) {
    Value V;
    V.I = I;
    return set(N, V);
  }
};

using Matches = SmallVector<std::pair<size_t, State>, 4>;

//===----------------------------------------------------------------------===//
// The parser
//===----------------------------------------------------------------------===//

class SHARCAsmParser : public MCTargetAsmParser {
  const MCRegisterInfo &MRI;
  const MCAsmInfo &AsmInfo;
  StringMap<std::pair<MCRegister, bool>> RegNames; // upper name -> (reg, isF)
  // Parsed templates, kept behind pointers so references stay valid while
  // matching parses more templates.
  DenseMap<const char *, std::unique_ptr<Seq>> TemplateCache;
  // Template literals are copied here: the lexer needs a NUL-terminated
  // buffer, and literal tokens point into it.
  BumpPtrAllocator Alloc;
  StringSaver Saver{Alloc};
  // Identifiers used as keywords in some template (MRF, LCE, DM, F, ...).
  // They may still be symbols, but a parse that avoids that is preferred.
  StringSet<> Reserved;
  SmallVector<unsigned, 64> Forms;
  SmallVector<unsigned, 400> Computes;

  // Per-statement state.
  SmallVector<AsmToken, 32> Toks;
  /// A computation matched at some position, with the fields of the
  /// enclosing form it sets ("^dataex").
  struct Nested {
    size_t Pos;
    MCInst *Inst;
    SmallVector<std::pair<std::string, Value>, 1> ParentFields;
    unsigned KeywordSymbols;
  };
  DenseMap<std::pair<unsigned, size_t>, SmallVector<Nested, 2>> NestedMemo;
  std::optional<MCInst> Parsed;

  // Hardware loops written with a label ("DO end UNTIL ..."), innermost
  // last. Each instruction after the DO is numbered; when the loop closes,
  // Count holds the number of instructions in it, so an instruction's
  // distance from the end is Count - index (see fixup_sharc_loop_end).
  struct Loop {
    MCSymbol *End;
    MCSymbol *Count;
    MCSymbol *Mode = nullptr;   // SHARC+ loop mode to infer
    SMLoc Loc;                  // of the end label in the DO
    unsigned Size = 0;          // instructions so far
    bool AtLast = false;        // the end label was just defined
    SmallVector<bool, 16> Flow; // instruction changes flow or idles
  };
  SmallVector<Loop, 2> Loops;

  // Section modes. A code section holds VISA code if it has SHF_SHARC_VISA,
  // or if VISA is the default (-mattr=+visa-section) and .isa was not used
  // in it. The visa-section feature tracks the current section's mode.
  bool DefaultVISA;
  SmallPtrSet<const MCSection *, 4> ExplicitISA;

  // Availability checks lifted while looking for a better diagnostic.
  enum Relax : unsigned {
    RelaxSHARCPlus = 1,
    RelaxFP64 = 2,
    RelaxVISASection = 4,
    RelaxVISA = 8,
  };
  unsigned Relaxed = 0;

public:
  SHARCAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                 const MCInstrInfo &MII)
      : MCTargetAsmParser(STI, MII),
        MRI(*Parser.getContext().getRegisterInfo()),
        AsmInfo(Parser.getContext().getAsmInfo()),
        DefaultVISA(STI.hasFeature(SHARC::FeatureVISASection)) {
    buildRegisterNames();
    for (unsigned Opc = 0; Opc < MII.getNumOpcodes(); ++Opc) {
      if (getSyntaxRows(Opc).empty())
        continue;
      if (isComputation(Opc))
        Computes.push_back(Opc);
      else
        Forms.push_back(Opc);
      for (const SyntaxRow &R : getSyntaxRows(Opc))
        reserveKeywords(getTemplate(R.Template));
    }
    for (const SyntaxRow &R : getCommonParts())
      reserveKeywords(getTemplate(R.Template));
  }

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override {
    return Error(StartLoc, "register expected");
  }
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override {
    return ParseStatus::NoMatch;
  }
  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;
  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;
  void convertToMapAndConstraints(unsigned Kind,
                                  const OperandVector &Operands) override {}
  ParseStatus parseDirective(AsmToken DirectiveID) override;
  void flushPendingInstructions(MCStreamer &Out) override {
    syncSectionMode();
  }
  void onLabelParsed(MCSymbol *Symbol) override {
    for (Loop &L : Loops)
      if (L.End == Symbol)
        L.AtLast = true;
  }
  void onEndOfFile() override {
    for (const Loop &L : Loops)
      Error(L.Loc, "hardware loop end '" + L.End->getName() +
                       "' is not defined in this section");
    Loops.clear();
  }

  /// "R0 = R1 + R2;" is an instruction, not a symbol assignment.
  bool equalIsAsmAssignment() override { return false; }
  /// "F3:2 = F5:4 + F9:8;" starts with a register pair, not a label.
  bool isLabel(AsmToken &Token) override {
    return !RegNames.count(Token.getString().upper());
  }

private:
  void buildRegisterNames();
  const Seq &getTemplate(const char *T);
  void reserveKeywords(const Seq &S) {
    for (const Node &N : S) {
      if (N.Kind == Node::Literal && N.Tok.is(AsmToken::Identifier))
        Reserved.insert(N.Tok.getString().upper());
      reserveKeywords(N.Children);
    }
  }
  Seq parseTemplate(StringRef T);

  bool available(unsigned Opc) const;
  MCSectionELF *codeSection();
  void trackLoops(MCInst &I, bool VISA, SMLoc Loc);
  MCStreamer &Out() { return getParser().getStreamer(); }
  bool syncSectionMode();
  SHARCTargetStreamer &targetStreamer() {
    return static_cast<SHARCTargetStreamer &>(
        *getParser().getStreamer().getTargetStreamer());
  }
  MCInst *matchStatement(unsigned OnlySize = 0);
  const FeatureBitset &features() const { return getSTI().getFeatureBits(); }

  void matchSeq(unsigned Opc, const Seq &S, size_t I, size_t Pos, State St,
                Matches &Out);
  void matchNode(unsigned Opc, const Node &N, size_t Pos, const State &St,
                 Matches &Out);
  void matchPart(unsigned Opc, StringRef Name, size_t Pos, const State &St,
                 Matches &Out);
  void matchList(unsigned Opc, const Node &N, size_t Pos, const State &St,
                 Matches &Out);
  void matchPlaceholder(unsigned Opc, const Node &N, size_t Pos,
                        const State &St, Matches &Out);
  bool applyConds(StringRef Conds, State &St) const;
  bool setAbsent(unsigned Opc, const Seq &S, State &St) const;

  std::optional<std::pair<MCRegister, bool>> regAt(size_t Pos) const;
  std::optional<std::pair<size_t, Value>> immAt(size_t Pos);
  ArrayRef<Nested> nestedAt(OpKind Kind, size_t Pos);
  unsigned fieldWidth(unsigned Opc, StringRef Name) const;
  MCInst *build(unsigned Opc, const State &St);
  bool isEnd(size_t Pos) const { return Pos >= Toks.size(); }
};

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Setup
//===----------------------------------------------------------------------===//

void SHARCAsmParser::buildRegisterNames() {
  for (unsigned R = 1; R < MRI.getNumRegs(); ++R)
    RegNames[StringRef(MRI.getName(R)).upper()] = {MCRegister(R), false};
  const MCRegisterClass &RF = MRI.getRegClass(SHARC::RFREGRegClassID);
  const MCRegisterClass &SF = MRI.getRegClass(SHARC::SREGRegClassID);
  for (unsigned I = 0; I < 16; ++I) {
    RegNames["F" + utostr(I)] = {RF.getRegister(I), true};
    RegNames["SF" + utostr(I)] = {SF.getRegister(I), true};
  }
}

const Seq &SHARCAsmParser::getTemplate(const char *T) {
  std::unique_ptr<Seq> &P = TemplateCache[T];
  if (!P)
    P = std::make_unique<Seq>(parseTemplate(T));
  return *P;
}

static size_t closing(StringRef T, size_t Open, char O, char C) {
  unsigned Depth = 0;
  for (size_t I = Open; I < T.size(); ++I) {
    if (T[I] == O)
      ++Depth;
    else if (T[I] == C && --Depth == 0)
      return I;
  }
  llvm_unreachable("unbalanced syntax template");
}

Seq SHARCAsmParser::parseTemplate(StringRef T) {
  Seq Out;
  auto Lit = [&](StringRef Text) {
    if (Text.trim().empty())
      return;
    AsmLexer L(AsmInfo);
    L.setBuffer(Saver.save(Text));
    for (L.Lex(); !L.is(AsmToken::Eof) && !L.is(AsmToken::EndOfStatement);
         L.Lex()) {
      Node N;
      N.Kind = Node::Literal;
      N.Tok = L.getTok();
      Out.push_back(N);
    }
  };
  size_t Start = 0;
  for (size_t I = 0; I < T.size();) {
    char C = T[I];
    if (C != '[' && C != '{' && C != '%' && C != '$') {
      ++I;
      continue;
    }
    Lit(T.slice(Start, I));
    Node N;
    if (C == '[' || C == '{') {
      size_t E = closing(T, I, C, C == '[' ? ']' : '}');
      StringRef Inner = T.slice(I + 1, E);
      if (C == '[') {
        N.Kind = Node::Optional;
        N.Children = parseTemplate(Inner);
      } else {
        N.Kind = Node::List;
        SmallVector<StringRef, 8> Names;
        Inner.split(Names, ',');
        for (StringRef P : Names) {
          Node Part;
          Part.Kind = Node::Part;
          Part.Name = P.trim().drop_front().str();
          N.Children.push_back(Part);
        }
      }
      I = E + 1;
    } else {
      size_t B = ++I;
      while (I < T.size() && (isAlnum(T[I]) || T[I] == '_'))
        ++I;
      N.Name = T.slice(B, I).str();
      N.Kind = C == '%' ? Node::Part : Node::Placeholder;
      if (C == '$' && I < T.size() && T[I] == '(' &&
          is_contained({"I", "M", "B", "Ix", "Bx", "cat", "bl6"}, N.Name)) {
        size_t E = T.find(')', I);
        SmallVector<StringRef, 3> Args;
        T.slice(I + 1, E).split(Args, ',');
        for (StringRef A : Args)
          N.Args.push_back(A.trim().str());
        I = E + 1;
      }
      if (C == '$' && I + 1 < T.size() && T[I] == ':' && isAlpha(T[I + 1])) {
        N.Fmt = T[I + 1];
        I += 2;
      }
    }
    Out.push_back(N);
    Start = I;
  }
  Lit(T.substr(Start));
  return Out;
}

/// Whether a form or computation exists for the current subtarget and section
/// mode: its TableGen predicates hold, and ISA code is all 48-bit.
bool SHARCAsmParser::available(unsigned Opc) const {
  FeatureBitset FB = features();
  if (Relaxed & RelaxSHARCPlus)
    FB.set(SHARC::FeatureSHARCPlus);
  if (Relaxed & RelaxFP64)
    FB.set(SHARC::FeatureFP64);
  if (Relaxed & RelaxVISA)
    FB.set(SHARC::FeatureVISA);
  bool VISASection = FB[SHARC::FeatureVISASection];
  if (Relaxed & RelaxVISASection)
    FB.reset(SHARC::FeatureVISASection);
  if (!isComputation(Opc) && MII.get(Opc).getSize() < 6 && !VISASection)
    return false;
  FeatureBitset Required = SHARC_MC::computeRequiredFeatures(Opc);
  return (SHARC_MC::computeAvailableFeatures(FB) & Required) == Required;
}

//===----------------------------------------------------------------------===//
// Matching
//===----------------------------------------------------------------------===//

void SHARCAsmParser::matchSeq(unsigned Opc, const Seq &S, size_t I, size_t Pos,
                              State St, Matches &Out) {
  if (I == S.size()) {
    Out.push_back({Pos, std::move(St)});
    return;
  }
  Matches Step;
  matchNode(Opc, S[I], Pos, St, Step);
  for (auto &[P, NS] : Step)
    matchSeq(Opc, S, I + 1, P, std::move(NS), Out);
}

bool SHARCAsmParser::applyConds(StringRef Conds, State &St) const {
  SmallVector<StringRef, 8> Terms;
  Conds.split(Terms, ',', -1, false);
  for (StringRef T : Terms) {
    T = T.trim();
    if (T == "@parse")
      continue;
    if (T.starts_with("+") || T.starts_with("-")) {
      bool Want = T[0] == '+';
      StringRef F = T.drop_front();
      unsigned Bit = StringSwitch<unsigned>(F)
                         .Case("sharcplus", SHARC::FeatureSHARCPlus)
                         .Case("visa", SHARC::FeatureVISA)
                         .Case("byte-addr", SHARC::FeatureByteAddr)
                         .Case("fp64", SHARC::FeatureFP64)
                         .Case("visa-section", SHARC::FeatureVISASection);
      if (features()[Bit] != Want)
        return false;
      continue;
    }
    auto [Name, V] = T.split('=');
    int64_t Val;
    if (V.getAsInteger(0, Val) || !St.setImm(Name.trim(), Val))
      return false;
  }
  return true;
}

void SHARCAsmParser::matchPart(unsigned Opc, StringRef Name, size_t Pos,
                               const State &St, Matches &Out) {
  auto Try = [&](ArrayRef<SyntaxRow> Rows) {
    bool Found = false;
    for (const SyntaxRow &R : Rows) {
      if (Name != R.Part)
        continue;
      Found = true;
      State NS = St;
      if (!applyConds(R.Conds, NS))
        continue;
      matchSeq(Opc, getTemplate(R.Template), 0, Pos, std::move(NS), Out);
    }
    return Found;
  };
  if (!Try(getSyntaxRows(Opc)))
    Try(getCommonParts());
}

/// A comma-separated list of optional fragments, each used at most once, in
/// the order written. Fragments not written take their empty row.
void SHARCAsmParser::matchList(unsigned Opc, const Node &N, size_t Pos,
                               const State &St, Matches &Out) {
  // Breadth-first over (next fragment index, position, state, any written).
  struct Item {
    size_t Next, Pos;
    State St;
    bool Any;
  };
  SmallVector<Item, 8> Work{{0, Pos, St, false}};
  while (!Work.empty()) {
    Item It = Work.pop_back_val();
    if (It.Next == N.Children.size()) {
      if (It.Any)
        Out.push_back({It.Pos, It.St});
      continue;
    }
    StringRef Part = N.Children[It.Next].Name;
    // Not written: the fragment's empty row.
    for (ArrayRef<SyntaxRow> Rows : {getSyntaxRows(Opc), getCommonParts()}) {
      for (const SyntaxRow &R : Rows) {
        if (Part != R.Part || *R.Template)
          continue;
        State NS = It.St;
        if (applyConds(R.Conds, NS))
          Work.push_back({It.Next + 1, It.Pos, NS, It.Any});
      }
    }
    // Written, after a comma unless it is the first.
    size_t P = It.Pos;
    if (It.Any) {
      if (isEnd(P) || !Toks[P].is(AsmToken::Comma))
        continue;
      ++P;
    }
    for (ArrayRef<SyntaxRow> Rows : {getSyntaxRows(Opc), getCommonParts()}) {
      for (const SyntaxRow &R : Rows) {
        if (Part != R.Part || !*R.Template)
          continue;
        State NS = It.St;
        if (!applyConds(R.Conds, NS))
          continue;
        Matches M;
        matchSeq(Opc, getTemplate(R.Template), 0, P, std::move(NS), M);
        for (auto &[MP, MS] : M)
          Work.push_back({It.Next + 1, MP, MS, true});
      }
    }
  }
}

/// Default values for the fields of an omitted optional group: no condition
/// (TRUE) and no computation.
bool SHARCAsmParser::setAbsent(unsigned Opc, const Seq &S, State &St) const {
  for (const Node &N : S) {
    if (N.Kind == Node::Literal)
      continue;
    if (N.Kind != Node::Placeholder)
      return false;
    const OperandInfo *O = findOperand(Opc, N.Name);
    if (!O)
      return false;
    if (O->Kind == OpKind::Cond && N.Name == "cond") {
      if (!St.setImm(N.Name, CondTrue))
        return false;
    } else if (O->Kind == OpKind::Compute) {
      if (!St.setImm(N.Name, 0))
        return false;
    } else {
      return false;
    }
  }
  return true;
}

void SHARCAsmParser::matchNode(unsigned Opc, const Node &N, size_t Pos,
                               const State &St, Matches &Out) {
  switch (N.Kind) {
  case Node::Literal: {
    if (isEnd(Pos))
      return;
    const AsmToken &T = Toks[Pos];
    bool Same = T.getKind() == N.Tok.getKind() &&
                (T.is(AsmToken::Identifier)
                     ? T.getString().equals_insensitive(N.Tok.getString())
                     : T.getString() == N.Tok.getString());
    if (Same)
      Out.push_back({Pos + 1, St});
    return;
  }
  case Node::Placeholder:
    matchPlaceholder(Opc, N, Pos, St, Out);
    return;
  case Node::Part:
    matchPart(Opc, N.Name, Pos, St, Out);
    return;
  case Node::List:
    matchList(Opc, N, Pos, St, Out);
    return;
  case Node::Optional: {
    matchSeq(Opc, N.Children, 0, Pos, St, Out);
    State NS = St;
    if (setAbsent(Opc, N.Children, NS))
      Out.push_back({Pos, std::move(NS)});
    return;
  }
  }
}

std::optional<std::pair<MCRegister, bool>>
SHARCAsmParser::regAt(size_t Pos) const {
  if (isEnd(Pos) || !Toks[Pos].is(AsmToken::Identifier))
    return std::nullopt;
  auto It = RegNames.find(Toks[Pos].getString().upper());
  if (It == RegNames.end())
    return std::nullopt;
  return It->second;
}

/// An integer, or a symbol optionally plus or minus an integer.
std::optional<std::pair<size_t, Value>> SHARCAsmParser::immAt(size_t Pos) {
  bool Neg = false;
  if (!isEnd(Pos) && Toks[Pos].is(AsmToken::Minus)) {
    Neg = true;
    ++Pos;
  }
  if (isEnd(Pos))
    return std::nullopt;
  Value V;
  if (Toks[Pos].is(AsmToken::Integer)) {
    V.I = Neg ? -Toks[Pos].getIntVal() : Toks[Pos].getIntVal();
    return std::make_pair(Pos + 1, V);
  }
  if (Neg || !Toks[Pos].is(AsmToken::Identifier) || regAt(Pos))
    return std::nullopt;
  MCContext &Ctx = getContext();
  const MCExpr *E = MCSymbolRefExpr::create(
      Ctx.getOrCreateSymbol(Toks[Pos].getString()), Ctx, Toks[Pos].getLoc());
  ++Pos;
  if (Pos + 1 < Toks.size() &&
      (Toks[Pos].is(AsmToken::Plus) || Toks[Pos].is(AsmToken::Minus)) &&
      Toks[Pos + 1].is(AsmToken::Integer)) {
    int64_t Off = Toks[Pos + 1].getIntVal();
    E = MCBinaryExpr::createAdd(
        E, MCConstantExpr::create(Toks[Pos].is(AsmToken::Minus) ? -Off : Off,
                                  Ctx),
        Ctx);
    Pos += 2;
  }
  V.Kind = Value::Expr;
  V.E = E;
  return std::make_pair(Pos, V);
}

/// The width of a field, or of a field of the enclosing form ("^dataex").
unsigned SHARCAsmParser::fieldWidth(unsigned Opc, StringRef Name) const {
  if (!Name.consume_front("^")) {
    const OperandInfo *O = findOperand(Opc, Name);
    assert(O && "syntax template names an unknown field");
    return O->Width;
  }
  for (unsigned F : Forms)
    if (const OperandInfo *O = findOperand(F, Name))
      return O->Width;
  llvm_unreachable("no form has the named field");
}

static bool inClass(const MCRegisterInfo &MRI, unsigned RC, MCRegister R) {
  return MRI.getRegClass(RC).contains(R);
}

/// Computations that can start at \p Pos, memoized per statement.
ArrayRef<SHARCAsmParser::Nested> SHARCAsmParser::nestedAt(OpKind Kind,
                                                          size_t Pos) {
  auto Key = std::make_pair(unsigned(Kind), Pos);
  auto It = NestedMemo.find(Key);
  if (It != NestedMemo.end())
    return It->second;
  SmallVector<Nested, 2> Found;
  for (unsigned Opc : Computes) {
    StringRef Name = MII.getName(Opc);
    bool Want = Kind == OpKind::ShortCompute ? Name.starts_with("SC_")
                : Kind == OpKind::ShiftImm   ? Name.starts_with("SI_")
                                             : Name.starts_with("C_");
    if (!Want || !available(Opc))
      continue;
    Matches M;
    matchSeq(Opc, getTemplate(getSyntaxRows(Opc)[0].Template), 0, Pos, State(),
             M);
    for (auto &[P, S] : M) {
      MCInst *I = build(Opc, S);
      if (!I)
        continue;
      Nested Nd{P, I, {}, S.KeywordSymbols};
      for (const auto &F : S.Fields)
        if (StringRef(F.first).starts_with("^"))
          Nd.ParentFields.push_back({F.first.substr(1), F.second});
      Found.push_back(std::move(Nd));
    }
  }
  return NestedMemo[Key] = Found;
}

void SHARCAsmParser::matchPlaceholder(unsigned Opc, const Node &N, size_t Pos,
                                      const State &St, Matches &Out) {
  // Registers derived from a DAG select bit and a 3-bit field.
  if (N.Name == "I" || N.Name == "M" || N.Name == "B" || N.Name == "Ix" ||
      N.Name == "Bx") {
    auto R = regAt(Pos);
    if (!R || R->second)
      return;
    unsigned RC = N.Name[0] == 'I'   ? SHARC::IREGRegClassID
                  : N.Name[0] == 'M' ? SHARC::MREGRegClassID
                                     : SHARC::BREGRegClassID;
    const MCRegisterClass &C = MRI.getRegClass(RC);
    unsigned Index = 0;
    while (Index < C.getNumRegs() && C.getRegister(Index) != R->first)
      ++Index;
    if (Index == C.getNumRegs())
      return;
    State NS = St;
    if (!NS.setImm(N.Args[0], Index / 8))
      return;
    if (N.Name.size() == 2)
      NS.Deferred.push_back({N.Args[0], N.Args[1], N.Args[2], Index});
    else if (!NS.setImm(N.Args[1], Index % 8))
      return;
    Out.push_back({Pos + 1, std::move(NS)});
    return;
  }
  if (N.Name == "cat" || N.Name == "bl6") {
    unsigned HiW = fieldWidth(Opc, N.Args[0]), LoW = fieldWidth(Opc, N.Args[1]);
    unsigned W = HiW + LoW;
    int64_t X;
    size_t Next;
    if (N.Name == "cat") {
      auto V = immAt(Pos);
      if (!V || V->second.Kind != Value::Imm)
        return;
      X = V->second.I;
      Next = V->first;
      if (!isIntN(W, X) && !isUIntN(W, X))
        return;
    } else {
      // <bit6>:<len6>.
      if (Pos + 2 >= Toks.size() || !Toks[Pos].is(AsmToken::Integer) ||
          !Toks[Pos + 1].is(AsmToken::Colon) ||
          !Toks[Pos + 2].is(AsmToken::Integer))
        return;
      int64_t Bit = Toks[Pos].getIntVal(), Len = Toks[Pos + 2].getIntVal();
      if (!isUInt<6>(Bit) || !isUInt<6>(Len))
        return;
      X = Len << 6 | Bit;
      Next = Pos + 3;
    }
    uint64_t U = uint64_t(X) & maskTrailingOnes<uint64_t>(W);
    State NS = St;
    if (NS.setImm(N.Args[0], U >> LoW) &&
        NS.setImm(N.Args[1], U & maskTrailingOnes<uint64_t>(LoW)))
      Out.push_back({Next, std::move(NS)});
    return;
  }

  const OperandInfo *O = findOperand(Opc, N.Name);
  assert(O && "syntax template names an unknown operand");
  switch (O->Kind) {
  case OpKind::Cond: {
    if (isEnd(Pos) || !Toks[Pos].is(AsmToken::Identifier))
      return;
    std::string Name = Toks[Pos].getString().upper();
    size_t Next = Pos + 1;
    if (Name == "NOT" && !isEnd(Next) && Toks[Next].is(AsmToken::Identifier)) {
      Name += " " + Toks[Next].getString().upper();
      ++Next;
    }
    std::optional<unsigned> C = parseCondName(Name, N.Name == "term");
    State NS = St;
    if (C && NS.setImm(N.Name, *C))
      Out.push_back({Next, std::move(NS)});
    return;
  }
  case OpKind::Compute:
  case OpKind::ShortCompute:
  case OpKind::ShiftImm:
    for (const Nested &Nd : nestedAt(O->Kind, Pos)) {
      State NS = St;
      Value V;
      V.Kind = Value::Inst;
      V.In = Nd.Inst;
      bool OK = NS.set(N.Name, V);
      NS.KeywordSymbols += Nd.KeywordSymbols;
      for (const auto &[F, FV] : Nd.ParentFields)
        OK = OK && NS.set(F, FV);
      if (OK)
        Out.push_back({Nd.Pos, std::move(NS)});
    }
    return;
  case OpKind::UImm:
  case OpKind::Imm:
  case OpKind::Addr:
  case OpKind::PCRel: {
    auto V = immAt(Pos);
    if (!V)
      return;
    // Symbols only where a fixup can place them (SHARCFixupKinds.h):
    // PC-relative offsets, 24-bit code addresses and 32-bit fields.
    if (V->second.Kind == Value::Expr &&
        !(O->Kind == OpKind::PCRel ||
          O->Width == 32 || (O->Kind == OpKind::Addr && O->Width == 24)))
      return;
    if (N.Fmt == 'L' && V->second.Kind != Value::Expr)
      return;
    bool Keyword = V->second.Kind == Value::Expr &&
                   Reserved.contains(Toks[Pos].getString().upper());
    if (V->second.Kind == Value::Imm) {
      int64_t X = V->second.I;
      unsigned W = O->Width;
      // Short data fields are sign-extended (SHARCUsage.rst, "Immediate
      // data"); only 32-bit data may be written either signed or unsigned.
      bool Fits = O->Kind == OpKind::PCRel ? isIntN(W, X)
                  : O->Kind == OpKind::Imm
                      ? (W < 32 ? isIntN(W, X) : isIntN(W, X) || isUIntN(W, X))
                      : isUIntN(W, X);
      if (!Fits)
        return;
      if (O->Kind != OpKind::PCRel)
        V->second.I = uint64_t(X) & maskTrailingOnes<uint64_t>(W);
    }
    State NS = St;
    NS.KeywordSymbols += Keyword;
    NS.NarrowSymbols +=
        V->second.Kind == Value::Expr && O->Kind == OpKind::PCRel && O->Width < 23;
    if (NS.set(N.Name, V->second))
      Out.push_back({V->first, std::move(NS)});
    return;
  }
  case OpKind::DBL: {
    // F(n+1):n with n even.
    if (Pos + 2 >= Toks.size() || !Toks[Pos + 1].is(AsmToken::Colon) ||
        !Toks[Pos + 2].is(AsmToken::Integer))
      return;
    auto Hi = regAt(Pos);
    int64_t Lo = Toks[Pos + 2].getIntVal();
    const MCRegisterClass &RF = MRI.getRegClass(SHARC::RFREGRegClassID);
    if (!Hi || !Hi->second || Lo < 0 || Lo > 14 || Lo % 2 ||
        RF.getRegister(Lo + 1) != Hi->first)
      return;
    Value V;
    V.Kind = Value::Reg;
    V.R = MRI.getRegClass(SHARC::DBLREGRegClassID).getRegister(Lo / 2);
    State NS = St;
    if (NS.set(N.Name, V))
      Out.push_back({Pos + 3, std::move(NS)});
    return;
  }
  default: {
    auto R = regAt(Pos);
    if (!R)
      return;
    auto [Reg, IsF] = *R;
    bool OK;
    switch (O->Kind) {
    case OpKind::RFREG:
      OK = !IsF && inClass(MRI, SHARC::RFREGRegClassID, Reg);
      break;
    case OpKind::FREG:
      OK = IsF && inClass(MRI, SHARC::RFREGRegClassID, Reg);
      break;
    case OpKind::SREG:
      OK = inClass(MRI, SHARC::SREGRegClassID, Reg);
      break;
    case OpKind::UREG:
      OK = inClass(MRI, SHARC::UREGRegClassID, Reg) ||
           (Reg == SHARC::MODE1STK && features()[SHARC::FeatureSHARCPlus]);
      break;
    case OpKind::SYSREG:
      OK = inClass(MRI, SHARC::SYSREGRegClassID, Reg) ||
           (Reg == SHARC::MODE1STK && features()[SHARC::FeatureSHARCPlus]);
      break;
    case OpKind::I1REG:
      OK = inClass(MRI, SHARC::I1REGRegClassID, Reg);
      break;
    case OpKind::I2REG:
      OK = inClass(MRI, SHARC::I2REGRegClassID, Reg);
      break;
    case OpKind::M1REG:
      OK = inClass(MRI, SHARC::M1REGRegClassID, Reg);
      break;
    case OpKind::M2REG:
      OK = inClass(MRI, SHARC::M2REGRegClassID, Reg);
      break;
    case OpKind::RXMREG:
    case OpKind::FXM:
      OK = (IsF == (O->Kind == OpKind::FXM)) &&
           inClass(MRI, SHARC::RXMREGRegClassID, Reg);
      break;
    case OpKind::RYMREG:
    case OpKind::FYM:
      OK = (IsF == (O->Kind == OpKind::FYM)) &&
           inClass(MRI, SHARC::RYMREGRegClassID, Reg);
      break;
    case OpKind::RXAREG:
    case OpKind::FXA:
      OK = (IsF == (O->Kind == OpKind::FXA)) &&
           inClass(MRI, SHARC::RXAREGRegClassID, Reg);
      break;
    case OpKind::RYAREG:
    case OpKind::FYA:
      OK = (IsF == (O->Kind == OpKind::FYA)) &&
           inClass(MRI, SHARC::RYAREGRegClassID, Reg);
      break;
    default:
      llvm_unreachable("not a register operand kind");
    }
    if (!OK)
      return;
    Value V;
    V.Kind = Value::Reg;
    V.R = Reg;
    State NS = St;
    if (NS.set(N.Name, V))
      Out.push_back({Pos + 1, std::move(NS)});
    return;
  }
  }
}

/// Build the MCInst for a complete match, resolving deferred destinations.
/// Fields the statement did not mention are zero.
MCInst *SHARCAsmParser::build(unsigned Opc, const State &In) {
  State St = In;
  for (const XorDest &D : St.Deferred) {
    const Value *A = St.get(D.A);
    if (!A || !St.setImm(D.G, D.Index / 8) ||
        !St.setImm(D.B, (D.Index % 8) ^ A->I))
      return nullptr;
  }
  MCInst *I = getContext().createMCInst();
  I->setOpcode(Opc);
  for (const OperandInfo &O : getOperandInfo(Opc)) {
    const Value *V = St.get(O.Name);
    if (!V) {
      if (O.Kind != OpKind::UImm && O.Kind != OpKind::Imm &&
          O.Kind != OpKind::Addr && O.Kind != OpKind::PCRel &&
          O.Kind != OpKind::Compute)
        return nullptr;
      I->addOperand(MCOperand::createImm(0));
      continue;
    }
    switch (V->Kind) {
    case Value::Imm:
      I->addOperand(MCOperand::createImm(V->I));
      break;
    case Value::Expr:
      I->addOperand(MCOperand::createExpr(V->E));
      break;
    case Value::Reg:
      I->addOperand(MCOperand::createReg(V->R));
      break;
    case Value::Inst:
      I->addOperand(MCOperand::createInst(V->In));
      break;
    }
  }
  return I;
}

//===----------------------------------------------------------------------===//
// Statements
//===----------------------------------------------------------------------===//

/// The current section if it holds code.
MCSectionELF *SHARCAsmParser::codeSection() {
  // SHARC objects are always ELF.
  auto *Sec = static_cast<MCSectionELF *>(
      getParser().getStreamer().getCurrentSectionOnly());
  if (!Sec || !(Sec->getFlags() & ELF::SHF_EXECINSTR))
    return nullptr;
  return Sec;
}

/// Set the visa-section feature from the current section. Returns whether
/// the section holds VISA code.
bool SHARCAsmParser::syncSectionMode() {
  bool VISA = false;
  if (MCSectionELF *Sec = codeSection())
    VISA = (Sec->getFlags() & ELF::SHF_SHARC_VISA) ||
           (DefaultVISA && !ExplicitISA.contains(Sec));
  if (VISA != getSTI().hasFeature(SHARC::FeatureVISASection))
    copySTI().ToggleFeature(SHARC::FeatureVISASection);
  return VISA;
}

bool SHARCAsmParser::parseInstruction(ParseInstructionInfo &Info,
                                      StringRef Name, SMLoc NameLoc,
                                      OperandVector &Operands) {
  // The generic parser has consumed the first identifier; re-collect it with
  // the rest of the statement.
  Toks.clear();
  NestedMemo.clear();
  Parsed.reset();
  Toks.push_back(AsmToken(AsmToken::Identifier,
                          StringRef(NameLoc.getPointer(), Name.size())));
  AsmLexer &Lexer = getLexer();
  while (!Lexer.is(AsmToken::EndOfStatement) && !Lexer.is(AsmToken::Eof)) {
    Toks.push_back(Lexer.getTok());
    Lexer.Lex();
  }
  SMLoc EndLoc = Lexer.getLoc();
  bool HasSemicolon =
      Lexer.is(AsmToken::EndOfStatement) && Lexer.getTok().getString() == ";";

  if (!HasSemicolon)
    return Error(EndLoc, "expected ';'");
  if (syncSectionMode()) {
    if (!getSTI().hasFeature(SHARC::FeatureVISA))
      return Error(NameLoc,
                   "VISA code requires an ADSP-214xx or SHARC+ processor");
    // Record the mode in the object when VISA is only the default.
    if (MCSectionELF *Sec = codeSection())
      Sec->setFlags(Sec->getFlags() | ELF::SHF_SHARC_VISA);
  }
  if (MCInst *I = matchStatement()) {
    trackLoops(*I, getSTI().hasFeature(SHARC::FeatureVISASection), NameLoc);
    Parsed = *I;
    return false;
  }
  // Explain a statement that would be valid elsewhere.
  static const std::pair<unsigned, const char *> Reasons[] = {
      {RelaxSHARCPlus, "instruction requires: sharcplus"},
      {RelaxFP64, "instruction requires: fp64"},
      {RelaxVISA, "instruction requires: visa"},
      {RelaxVISASection, "Type 10 instructions are not available in VISA code"},
  };
  for (auto [R, Msg] : Reasons) {
    Relaxed = R;
    NestedMemo.clear();
    bool Found = matchStatement();
    Relaxed = 0;
    NestedMemo.clear();
    if (Found)
      return Error(NameLoc, Msg);
  }
  return Error(NameLoc, "invalid SHARC instruction");
}

static bool changesFlow(StringRef Name) {
  return Name.starts_with("T8a") || Name.starts_with("T9") ||
         Name.starts_with("T10a") || Name.starts_with("T11") ||
         Name == "T25a_direct" || Name == "T25a_pcrel" ||
         Name.starts_with("T22") || Name.starts_with("T12a") ||
         Name == "T13a";
}

/// Hardware loops: number \p I within each open loop, attach its loop
/// positions and 48-bit alternative, close the loops it ends, and open a
/// loop if it is a DO with a label.
void SHARCAsmParser::trackLoops(MCInst &I, bool VISA, SMLoc Loc) {
  MCContext &Ctx = getContext();
  StringRef Name = MII.getName(I.getOpcode());
  bool Narrow = MII.get(I.getOpcode()).getSize() < 6;

  // Positions in the open loops; ISA code is all 48-bit already.
  SmallVector<const MCExpr *, 2> Positions;
  for (Loop &L : Loops) {
    if (VISA && Narrow)
      Positions.push_back(MCBinaryExpr::create(
          MCBinaryExpr::Sub, MCSymbolRefExpr::create(L.Count, Ctx),
          MCConstantExpr::create(L.Size, Ctx), Ctx, Loc));
    ++L.Size;
    L.Flow.push_back(changesFlow(Name));
  }

  // A 48-bit alternative for a narrow instruction that may have to grow:
  // near a loop end, or with a label in a 6-bit offset.
  bool NarrowSymbol = false;
  for (const OperandInfo &O : getOperandInfo(I.getOpcode()))
    if (O.Kind == OpKind::PCRel && O.Width < 23 &&
        I.getOperand(O.Index).isExpr())
      NarrowSymbol = true;
  MCInst *Wide = nullptr;
  if (Narrow && (!Positions.empty() || NarrowSymbol)) {
    NestedMemo.clear();
    Wide = matchStatement(6);
  }
  for (const MCExpr *E : Positions)
    I.addOperand(MCOperand::createExpr(E));
  if (Wide)
    I.addOperand(MCOperand::createInst(Wide));

  // Close the loops this instruction ends, innermost first. An inner loop's
  // end is a change of flow in the loops around it.
  while (!Loops.empty() && Loops.back().AtLast) {
    Loop L = Loops.pop_back_val();
    Out().emitAssignment(L.Count, MCConstantExpr::create(L.Size, Ctx));
    if (L.Mode) {
      // F1-active, (F), is safe without a branch or IDLE in the last eleven
      // instructions (PRM 4-41 text); E2-active otherwise.
      bool F = none_of(ArrayRef(L.Flow).take_back(11),
                       [](bool B) { return B; });
      Out().emitAssignment(L.Mode, MCConstantExpr::create(F, Ctx));
    }
    if (!Loops.empty())
      Loops.back().Flow.back() = true;
  }

  // A DO with a label opens a loop.
  if (!Name.starts_with("T12a") && Name != "T13a")
    return;
  const MCOperand &Target =
      I.getOperand(findOperand(I.getOpcode(), "reladdr")->Index);
  const auto *Ref =
      Target.isExpr() ? dyn_cast<MCSymbolRefExpr>(Target.getExpr()) : nullptr;
  if (!Ref)
    return;
  Loop L;
  L.End = const_cast<MCSymbol *>(&Ref->getSymbol());
  L.Count = Ctx.createTempSymbol("sharc_loop_size", true);
  L.Loc = Ref->getLoc();
  // On SHARC+, infer the mode of a counter loop written without (F).
  MCOperand &Mode = I.getOperand(findOperand(I.getOpcode(), "mode")->Index);
  if (Name.starts_with("T12a") && getSTI().hasFeature(SHARC::FeatureSHARCPlus) &&
      Mode.isImm() && Mode.getImm() == 0) {
    L.Mode = Ctx.createTempSymbol("sharc_loop_mode", true);
    Mode = MCOperand::createExpr(MCSymbolRefExpr::create(L.Mode, Ctx));
  }
  Loops.push_back(std::move(L));
}

/// Of the forms that match the whole statement, the one reading the fewest
/// keywords as symbols, then the smallest.
MCInst *SHARCAsmParser::matchStatement(unsigned OnlySize) {
  MCInst *Best = nullptr;
  std::tuple<unsigned, unsigned, unsigned> BestKey(~0u, ~0u, ~0u);
  for (unsigned Opc : Forms) {
    if (!available(Opc) ||
        (OnlySize && MII.get(Opc).getSize() != OnlySize))
      continue;
    for (const SyntaxRow &R : getSyntaxRows(Opc)) {
      if (*R.Part)
        continue;
      State St;
      if (!applyConds(R.Conds, St))
        continue;
      Matches M;
      matchSeq(Opc, getTemplate(R.Template), 0, 0, std::move(St), M);
      for (auto &[P, S] : M) {
        if (P != Toks.size())
          continue;
        std::tuple<unsigned, unsigned, unsigned> Key(
            S.KeywordSymbols, MII.get(Opc).getSize(), S.NarrowSymbols);
        if (Key >= BestKey)
          continue;
        if (MCInst *I = build(Opc, S)) {
          Best = I;
          BestKey = Key;
        }
      }
    }
  }
  return Best;
}

bool SHARCAsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                             OperandVector &Operands,
                                             MCStreamer &Out,
                                             uint64_t &ErrorInfo,
                                             bool MatchingInlineAsm) {
  assert(Parsed && "parseInstruction must succeed first");
  Parsed->setLoc(IDLoc);
  Out.emitInstruction(*Parsed, getSTI());
  Parsed.reset();
  return false;
}

/// .word: 32-bit data words, like .4byte.
/// .isa / .visa: the current section holds ISA or VISA code.
ParseStatus SHARCAsmParser::parseDirective(AsmToken DirectiveID) {
  StringRef Dir = DirectiveID.getString();
  if (Dir == ".isa" || Dir == ".visa") {
    if (parseEOL())
      return ParseStatus::Failure;
    bool VISA = Dir == ".visa";
    if (VISA && !getSTI().hasFeature(SHARC::FeatureVISA))
      return Error(DirectiveID.getLoc(),
                   ".visa requires an ADSP-214xx or SHARC+ processor");
    auto *Sec = static_cast<MCSectionELF *>(
        getParser().getStreamer().getCurrentSectionOnly());
    if (!Sec)
      return Error(DirectiveID.getLoc(), Dir + " outside a section");
    if (VISA) {
      Sec->setFlags(Sec->getFlags() | ELF::SHF_SHARC_VISA);
      ExplicitISA.erase(Sec);
      targetStreamer().emitDirectiveVISA();
    } else {
      Sec->setFlags(Sec->getFlags() & ~ELF::SHF_SHARC_VISA);
      ExplicitISA.insert(Sec);
      targetStreamer().emitDirectiveISA();
    }
    syncSectionMode();
    return ParseStatus::Success;
  }
  if (Dir != ".word")
    return ParseStatus::NoMatch;
  auto ParseOne = [&]() -> bool {
    const MCExpr *Value;
    SMLoc Loc = getLexer().getLoc();
    if (getParser().parseExpression(Value))
      return true;
    getParser().getStreamer().emitValue(Value, 4, Loc);
    return false;
  };
  return getParser().parseMany(ParseOne);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSHARCAsmParser() {
  RegisterMCAsmParser<SHARCAsmParser> X(getTheSHARCTarget());
}
