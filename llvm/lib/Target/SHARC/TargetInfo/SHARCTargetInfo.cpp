//===-- SHARCTargetInfo.cpp - SHARC target info ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/SHARCTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

Target &llvm::getTheSHARCTarget() {
  static Target TheSHARCTarget;
  return TheSHARCTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeSHARCTargetInfo() {
  RegisterTarget<Triple::sharc> X(getTheSHARCTarget(), "sharc",
                                  "Analog Devices SHARC", "SHARC");
}
