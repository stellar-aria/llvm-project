//===-- SHARCSyntax.h - SHARC instruction syntax tables ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// SHARC assembly is algebraic and has no mnemonics, so the printer and the
// parser share one description of each instruction's syntax:
//
// - SHARCOperandInfo.def names every MCInst operand of every form and
//   computation, with its kind and field width;
// - SHARCFormSyntax.def gives each form's syntax as templates selected by
//   field values (transcribed from the PRM's encode tables);
// - SHARCComputeSyntax.def gives each computation's template.
//
// Template language:
//   $name[:f]     operand by name; f = x (hex), s (signed decimal),
//                 h (signed hex), d (decimal), L (parse a symbol only)
//   $I(g,f)       index register I(8*g + f); likewise $M and $B
//   $Ix(g,a,b)    index register I(8*g + (a ^ b)); likewise $Bx
//   $cat(h,l)[:f] immediate formed from fields h:l (sign-extended)
//   %Part         a fragment chosen from SHARC_PART rows by field values
//   [ ... ]       optional: printed when every placeholder inside is present
//   {%A, %B, ...} list of optional fragments, joined by ", "
// Anything else is literal text.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCSYNTAX_H
#define LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCSYNTAX_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCRegister.h"
#include <cstdint>
#include <optional>

namespace llvm {
class FeatureBitset;
class MCInst;
class MCRegisterInfo;

namespace SHARC {

enum class OpKind : uint8_t {
  UImm,
  Imm,
  Addr,
  PCRel,
  Cond,
  Compute,
  ShortCompute,
  ShiftImm,
  RFREG,
  FREG,
  SREG,
  UREG,
  SYSREG,
  I1REG,
  I2REG,
  M1REG,
  M2REG,
  RXMREG,
  RYMREG,
  RXAREG,
  RYAREG,
  FXM,
  FYM,
  FXA,
  FYA,
  DBL,
};

struct OperandInfo {
  const char *Name;
  uint8_t Index;
  OpKind Kind;
  uint8_t Width;
};

/// The operands of a form or computation, in MCInst order.
ArrayRef<OperandInfo> getOperandInfo(unsigned Opcode);
const OperandInfo *findOperand(unsigned Opcode, StringRef Name);

struct SyntaxRow {
  const char *Part; ///< Empty for a top-level row.
  const char *Conds;
  const char *Template;
};

/// The syntax rows of an opcode: SHARC_SYNTAX and SHARC_PART rows for a
/// form, or a single row for a computation.
ArrayRef<SyntaxRow> getSyntaxRows(unsigned Opcode);

/// Fragments shared by all forms (SHARC_COMMON_PART rows). A form's own
/// SHARC_PART rows are searched first.
ArrayRef<SyntaxRow> getCommonParts();

/// True if \p Opcode is a computation nested in a form, not a form.
bool isComputation(unsigned Opcode);

/// Evaluate "f=v,g=w,+feature,-feature" against \p MI's fields and \p FB.
/// A row marked "@parse" is for the parser only and never holds here.
bool condsHold(const MCInst &MI, StringRef Conds, const FeatureBitset &FB);

/// Field value of a non-register operand, by name.
std::optional<int64_t> getField(const MCInst &MI, StringRef Name);

/// Condition (IF) or termination (DO UNTIL) code names (PGR Table 10-4,
/// PRM Table 4-20). Code 31 is TRUE / FOREVER; code 14 is BM on the
/// ADSP-2136x/2137x and SF on later cores.
StringRef condName(unsigned Code, bool Term, const FeatureBitset &FB);
std::optional<unsigned> parseCondName(StringRef Name, bool Term);

/// The register class index base for DAG-relative fields.
MCRegister dagRegister(const MCRegisterInfo &MRI, char Kind, unsigned Index);

constexpr unsigned CondTrue = 31;

} // namespace SHARC
} // namespace llvm

#endif // LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCSYNTAX_H
