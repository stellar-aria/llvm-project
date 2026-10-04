//===-- SHARCTargetMachine.h - SHARC target machine -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Only the MC layer is implemented. The target machine exists so the target
// registers like any other; it runs no code generation passes yet.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SHARC_SHARCTARGETMACHINE_H
#define LLVM_LIB_TARGET_SHARC_SHARCTARGETMACHINE_H

#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include <optional>

namespace llvm {

class SHARCTargetMachine : public CodeGenTargetMachineImpl {
public:
  SHARCTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                     StringRef FS, const TargetOptions &Options,
                     std::optional<Reloc::Model> RM,
                     std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                     bool JIT);
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_SHARC_SHARCTARGETMACHINE_H
