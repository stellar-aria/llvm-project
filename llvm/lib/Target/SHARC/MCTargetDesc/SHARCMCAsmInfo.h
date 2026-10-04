//===-- SHARCMCAsmInfo.h - SHARC assembly syntax --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCMCASMINFO_H
#define LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class SHARCMCAsmInfo : public MCAsmInfoELF {
public:
  explicit SHARCMCAsmInfo(const Triple &TT, const MCTargetOptions &Options);
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCMCASMINFO_H
