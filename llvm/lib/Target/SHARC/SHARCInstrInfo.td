//===-- SHARCInstrInfo.td - SHARC instructions -------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Placeholder until the generated instruction forms are imported.
//
//===----------------------------------------------------------------------===//

def NOP_placeholder : Instruction {
  let Namespace = "SHARC";
  let OutOperandList = (outs);
  let InOperandList = (ins);
  let AsmString = "NOP;";
  let Size = 6;
  let hasSideEffects = 1;
}
