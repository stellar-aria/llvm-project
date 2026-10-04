//===-- SHARCTargetInfo.h - SHARC target info -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SHARC_TARGETINFO_SHARCTARGETINFO_H
#define LLVM_LIB_TARGET_SHARC_TARGETINFO_SHARCTARGETINFO_H

namespace llvm {

class Target;

Target &getTheSHARCTarget();

} // end namespace llvm

#endif // LLVM_LIB_TARGET_SHARC_TARGETINFO_SHARCTARGETINFO_H
