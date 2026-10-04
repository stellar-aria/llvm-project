//===-- SHARC.td - Describe the SHARC target machine -------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Subtarget features
//===----------------------------------------------------------------------===//

def FeatureVISA : SubtargetFeature<"visa", "HasVISA", "true",
    "16/32/48-bit VISA encodings in short-word space">;
def FeatureByteAddr : SubtargetFeature<"byte-addr", "HasByteAddr", "true",
    "Byte address space access modifiers and B2W/W2B">;
def FeatureFP64 : SubtargetFeature<"fp64", "HasFP64", "true",
    "64-bit floating point on register pairs">;
def FeatureSHARCPlus : SubtargetFeature<"sharcplus", "IsSHARCPlus", "true",
    "SHARC+ core", [FeatureVISA, FeatureByteAddr, FeatureFP64]>;

// Internal: the code being assembled or disassembled is linked into
// short-word space (a section with SHF_SHARC_VISA). The same bits decode
// differently in ISA and VISA code (PGR p443).
def FeatureVISASection : SubtargetFeature<"visa-section", "InVISASection",
    "true", "Code is in a VISA (short-word space) section">;

def NotInVISASection : Predicate<"!Subtarget->inVISASection()">,
                       AssemblerPredicate<(all_of (not FeatureVISASection))>;

def HasVISA : Predicate<"Subtarget->hasVISA()">,
              AssemblerPredicate<(all_of FeatureVISA)>;
def IsSHARCPlus : Predicate<"Subtarget->isSHARCPlus()">,
                  AssemblerPredicate<(all_of FeatureSHARCPlus)>;
def IsClassic : Predicate<"!Subtarget->isSHARCPlus()">,
                AssemblerPredicate<(all_of (not FeatureSHARCPlus))>;
def HasByteAddr : Predicate<"Subtarget->hasByteAddr()">,
                  AssemblerPredicate<(all_of FeatureByteAddr)>;
def HasFP64 : Predicate<"Subtarget->hasFP64()">,
              AssemblerPredicate<(all_of FeatureFP64)>;

//===----------------------------------------------------------------------===//
// Registers and instructions
//===----------------------------------------------------------------------===//

include "SHARCRegisterInfo.td"
include "SHARCInstrInfo.td"

// DAG index registers hold addresses. Revisit with code generation (M4).
defm : RemapAllTargetPseudoPointerOperands<IREG>;

def SHARCInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Processors
//===----------------------------------------------------------------------===//

def : ProcessorModel<"adsp-2136x", NoSchedModel, []>;
def : ProcessorModel<"adsp-2137x", NoSchedModel, []>;
def : ProcessorModel<"adsp-214xx", NoSchedModel, [FeatureVISA]>;
def : ProcessorModel<"adsp-2156x", NoSchedModel, [FeatureSHARCPlus]>;
def : ProcessorModel<"adsp-sc58x", NoSchedModel, [FeatureSHARCPlus]>;
def : ProcessorModel<"adsp-2158x", NoSchedModel, [FeatureSHARCPlus]>;

//===----------------------------------------------------------------------===//
// Target
//===----------------------------------------------------------------------===//

def SHARC : Target {
  let InstructionSet = SHARCInstrInfo;
}
