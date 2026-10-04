//===-- SHARCMCTargetDesc.h - SHARC target descriptions -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCMCTARGETDESC_H
#define LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCMCTARGETDESC_H

// Defines symbolic names for SHARC registers.
#define GET_REGINFO_ENUM
#include "SHARCGenRegisterInfo.inc"

// Defines symbolic names for the SHARC instructions.
#define GET_INSTRINFO_ENUM
#include "SHARCGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "SHARCGenSubtargetInfo.inc"

#endif // LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCMCTARGETDESC_H
