//===-- SHARCTargetStreamer.h - SHARC target streamer -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCTARGETSTREAMER_H
#define LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCTARGETSTREAMER_H

#include "llvm/MC/MCStreamer.h"

namespace llvm {
class formatted_raw_ostream;
class MCSubtargetInfo;

class SHARCTargetStreamer : public MCTargetStreamer {
public:
  SHARCTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

  /// .isa / .visa: the current section's code mode. The section flag is set
  /// by the parser; only assembly output needs the directive itself.
  virtual void emitDirectiveISA() {}
  virtual void emitDirectiveVISA() {}
};

class SHARCTargetAsmStreamer : public SHARCTargetStreamer {
  formatted_raw_ostream &OS;

public:
  SHARCTargetAsmStreamer(MCStreamer &S, formatted_raw_ostream &OS)
      : SHARCTargetStreamer(S), OS(OS) {}
  void emitDirectiveISA() override;
  void emitDirectiveVISA() override;
};

/// Sets e_flags from the processor (EF_SHARC_MACH_*).
class SHARCTargetELFStreamer : public SHARCTargetStreamer {
public:
  SHARCTargetELFStreamer(MCStreamer &S, const MCSubtargetInfo &STI);
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCTARGETSTREAMER_H
