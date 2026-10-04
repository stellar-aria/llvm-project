//===-- SHARCOperands.td - SHARC instruction operands ------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Operand classes used by the generated instruction forms and computations.
// Register fields encode the low bits of the register's Ureg code, which the
// generated emitter masks to the field width.
//
//===----------------------------------------------------------------------===//

// Data registers printed with their floating-point names (F0-F15).
class SHARCFRegOperand<RegisterClass rc> : RegisterOperand<rc> {
  let PrintMethod = "printFReg";
}
def FREG_op : SHARCFRegOperand<RFREG>;
def FXM_op : SHARCFRegOperand<RXMREG>;
def FYM_op : SHARCFRegOperand<RYMREG>;
def FXA_op : SHARCFRegOperand<RXAREG>;
def FYA_op : SHARCFRegOperand<RYAREG>;

// 64-bit register pair F(n+1):n, encoded as the even register number.
def DBL_op : RegisterOperand<DBLREG> {
  let PrintMethod = "printDblReg";
  let EncoderMethod = "getDblRegOpValue";
  let DecoderMethod = "decodeDblRegOperand";
}

// The computation nested in a form's compute field (an MCInst operand).
class SHARCNestedOperand<string kind> : Operand<i32> {
  let PrintMethod = "print"#kind#"Operand";
  let EncoderMethod = "get"#kind#"OpValue";
  let DecoderMethod = "decode"#kind#"Operand";
  let OperandType = "OPERAND_IMMEDIATE";
}
def compute_op : SHARCNestedOperand<"Compute">;
def scompute_op : SHARCNestedOperand<"ShortCompute">;
def shiftimm_op : SHARCNestedOperand<"ShiftImm">;

// IF condition or DO UNTIL termination code (PGR Table 10-4).
def cond_op : Operand<i32> {
  let PrintMethod = "printCondOperand";
  let DecoderMethod = "decodeUImmOperand<5>";
}

// Unsigned fields: flag bits and selectors.
foreach n = [1, 3, 4, 8, 16] in
  def uimm#n#_op : Operand<i32> {
    let DecoderMethod = "decodeUImmOperand<"#n#">";
  }

// Immediate data, possibly an expression resolved by a fixup.
foreach n = [6, 7, 8, 16, 32] in
  def imm#n#_op : Operand<i32> {
    let EncoderMethod = "getImmOpValue";
    let DecoderMethod = "decodeImmOperand<"#n#">";
  }

// Absolute program or data addresses.
foreach n = [24, 32] in
  def addr#n#_op : Operand<i32> {
    let EncoderMethod = "getImmOpValue";
    let DecoderMethod = "decodeUImmOperand<"#n#">";
  }

// PC-relative offsets, in instructions (ISA) or 16-bit parcels (VISA).
foreach n = [6, 23, 24] in
  def pcrel#n#_op : Operand<i32> {
    let EncoderMethod = "getPCRelOpValue";
    let DecoderMethod = "decodePCRelOperand<"#n#">";
    let OperandType = "OPERAND_PCREL";
  }
