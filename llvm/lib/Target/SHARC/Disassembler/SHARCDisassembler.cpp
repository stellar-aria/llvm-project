//===-- SHARCDisassembler.cpp - Disassembler for SHARC --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ISA code is a sequence of 48-bit instructions. VISA code (ADSP-214xx and
// SHARC+, in short-word space) mixes 16-, 32- and 48-bit instructions; the
// length is decoded from the leading bits of a 48-bit window (PGR p446).
// Computations nested in an instruction's compute field are decoded from
// their own tables into an MCInst operand.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/SHARCMCTargetDesc.h"
#include "MCTargetDesc/SHARCParcels.h"
#include "TargetInfo/SHARCTargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;
using namespace llvm::MCD;

#define DEBUG_TYPE "sharc-disassembler"

using DecodeStatus = MCDisassembler::DecodeStatus;

namespace {

class SHARCDisassembler : public MCDisassembler {
public:
  SHARCDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &MI, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CS) const override;
};

} // end anonymous namespace

static MCDisassembler *createSHARCDisassembler(const Target &T,
                                               const MCSubtargetInfo &STI,
                                               MCContext &Ctx) {
  return new SHARCDisassembler(STI, Ctx);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSHARCDisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheSHARCTarget(),
                                         createSHARCDisassembler);
}

//===----------------------------------------------------------------------===//
// Operand decoders
//===----------------------------------------------------------------------===//

static DecodeStatus decodeRegInClass(MCInst &Inst, unsigned RC, uint64_t Index,
                                     const MCDisassembler *D) {
  const MCRegisterClass &Class =
      D->getContext().getRegisterInfo()->getRegClass(RC);
  if (Index >= Class.getNumRegs())
    return MCDisassembler::Fail;
  Inst.addOperand(MCOperand::createReg(Class.getRegister(Index)));
  return MCDisassembler::Success;
}

#define SHARC_REG_DECODER(CLASS)                                               \
  static DecodeStatus Decode##CLASS##RegisterClass(                            \
      MCInst &Inst, uint64_t Index, uint64_t Address,                          \
      const MCDisassembler *D) {                                               \
    return decodeRegInClass(Inst, SHARC::CLASS##RegClassID, Index, D);         \
  }
SHARC_REG_DECODER(RFREG)
SHARC_REG_DECODER(SREG)
SHARC_REG_DECODER(I1REG)
SHARC_REG_DECODER(I2REG)
SHARC_REG_DECODER(M1REG)
SHARC_REG_DECODER(M2REG)
SHARC_REG_DECODER(RXMREG)
SHARC_REG_DECODER(RYMREG)
SHARC_REG_DECODER(RXAREG)
SHARC_REG_DECODER(RYAREG)
#undef SHARC_REG_DECODER

/// Universal register code (PGR Table 10-2). 0x62 is reserved. 0x7D is
/// LIRPTL on classic cores and MODE1STK on SHARC+ (PRM p551).
static DecodeStatus DecodeUREGRegisterClass(MCInst &Inst, uint64_t Code,
                                            uint64_t Address,
                                            const MCDisassembler *D) {
  if (Code == 0x7D && D->getSubtargetInfo().hasFeature(SHARC::FeatureSHARCPlus)) {
    Inst.addOperand(MCOperand::createReg(SHARC::MODE1STK));
    return MCDisassembler::Success;
  }
  const MCRegisterInfo *MRI = D->getContext().getRegisterInfo();
  for (MCRegister R : MRI->getRegClass(SHARC::UREGRegClassID)) {
    if (MRI->getEncodingValue(R) == Code) {
      Inst.addOperand(MCOperand::createReg(R));
      return MCDisassembler::Success;
    }
  }
  return MCDisassembler::Fail;
}

static DecodeStatus DecodeSYSREGRegisterClass(MCInst &Inst, uint64_t Index,
                                              uint64_t Address,
                                              const MCDisassembler *D) {
  return DecodeUREGRegisterClass(Inst, 0x70 | Index, Address, D);
}

/// 64-bit register pair F(n+1):n, encoded as the even register number. An
/// odd code names a single register, which a pair operand does not accept
/// (PRM Table 18-28).
static DecodeStatus decodeDblRegOperand(MCInst &Inst, uint64_t Code,
                                        uint64_t Address,
                                        const MCDisassembler *D) {
  if (Code & 1)
    return MCDisassembler::Fail;
  return decodeRegInClass(Inst, SHARC::DBLREGRegClassID, Code / 2, D);
}

template <unsigned N>
static DecodeStatus decodeUImmOperand(MCInst &Inst, uint64_t Imm,
                                      uint64_t Address,
                                      const MCDisassembler *D) {
  assert(isUInt<N>(Imm) && "field wider than its operand");
  Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

/// Data fields are kept as the raw field value; how a field extends to 32
/// bits depends on the instruction and is the printer's concern.
template <unsigned N>
static DecodeStatus decodeImmOperand(MCInst &Inst, uint64_t Imm,
                                     uint64_t Address,
                                     const MCDisassembler *D) {
  return decodeUImmOperand<N>(Inst, Imm, Address, D);
}

/// PC-relative offsets are two's complement, in instructions (ISA) or
/// parcels (VISA).
template <unsigned N>
static DecodeStatus decodePCRelOperand(MCInst &Inst, uint64_t Imm,
                                       uint64_t Address,
                                       const MCDisassembler *D) {
  Inst.addOperand(MCOperand::createImm(SignExtend64<N>(Imm)));
  return MCDisassembler::Success;
}

static DecodeStatus decodeNested(MCInst &Inst, const uint8_t *Table,
                                 uint64_t Field, uint64_t Address,
                                 const MCDisassembler *D);

/// A form's 23-bit compute field. All zeros means "no computation".
static DecodeStatus decodeComputeOperand(MCInst &Inst, uint64_t Field,
                                         uint64_t Address,
                                         const MCDisassembler *D);
static DecodeStatus decodeShortComputeOperand(MCInst &Inst, uint64_t Field,
                                              uint64_t Address,
                                              const MCDisassembler *D);
static DecodeStatus decodeShiftImmOperand(MCInst &Inst, uint64_t Field,
                                          uint64_t Address,
                                          const MCDisassembler *D);

// 48-bit forms are decoded from a uint64_t, 16/32-bit forms and the 24-bit
// computations from a uint32_t.
#include "SHARCGenDisassemblerTables.inc"

static DecodeStatus decodeNested(MCInst &Inst, const uint8_t *Table,
                                 uint64_t Field, uint64_t Address,
                                 const MCDisassembler *D) {
  MCContext &Ctx = D->getContext();
  MCInst *Sub = Ctx.createMCInst();
  DecodeStatus S = decodeInstruction(Table, *Sub, uint32_t(Field), Address, D,
                                     D->getSubtargetInfo());
  if (S == MCDisassembler::Fail)
    return S;
  Inst.addOperand(MCOperand::createInst(Sub));
  return S;
}

static DecodeStatus decodeComputeOperand(MCInst &Inst, uint64_t Field,
                                         uint64_t Address,
                                         const MCDisassembler *D) {
  if (Field == 0) {
    Inst.addOperand(MCOperand::createImm(0));
    return MCDisassembler::Success;
  }
  return decodeNested(Inst, DecoderTableCompute24, Field, Address, D);
}

static DecodeStatus decodeShortComputeOperand(MCInst &Inst, uint64_t Field,
                                              uint64_t Address,
                                              const MCDisassembler *D) {
  return decodeNested(Inst, DecoderTableShortCompute16, Field, Address, D);
}

static DecodeStatus decodeShiftImmOperand(MCInst &Inst, uint64_t Field,
                                          uint64_t Address,
                                          const MCDisassembler *D) {
  return decodeNested(Inst, DecoderTableShiftImm24, Field, Address, D);
}

//===----------------------------------------------------------------------===//
// Instruction decoding
//===----------------------------------------------------------------------===//

namespace {
struct FormLength {
  uint64_t Mask, Value;
  uint8_t Width, Lead, Fixed;
};

const FormLength VisaForms[] = {
#define SHARC_FORM(NAME, WIDTH, MASK, VALUE, LEAD, FIXED)                      \
  {MASK, VALUE, WIDTH, LEAD, FIXED},
#include "SHARCFormLengths.def"
#undef SHARC_FORM
};
} // namespace

/// The length of the VISA instruction at the top of a 48-bit window: among
/// the forms whose fixed bits match, the one with the longest run of fixed
/// bits from bit 47 wins, then the one with the most fixed bits. Returns 0
/// if nothing matches.
static unsigned visaLength(uint64_t W) {
  const FormLength *Best = nullptr;
  for (const FormLength &F : VisaForms) {
    if ((W & F.Mask) != F.Value)
      continue;
    if (!Best || F.Lead > Best->Lead ||
        (F.Lead == Best->Lead && F.Fixed > Best->Fixed))
      Best = &F;
  }
  return Best ? Best->Width : 0;
}

DecodeStatus SHARCDisassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                               ArrayRef<uint8_t> Bytes,
                                               uint64_t Address,
                                               raw_ostream &CS) const {
  unsigned Avail;
  uint64_t W = SHARC::readWindow(Bytes, Avail);
  // A failed decode skips one parcel, the smallest instruction unit.
  Size = std::min(Avail, 1u) * 2;

  unsigned Width = 48;
  if (STI.hasFeature(SHARC::FeatureVISASection)) {
    Width = visaLength(W);
    if (Width == 0)
      return MCDisassembler::Fail;
  }
  if (Width / 16 > Avail)
    return MCDisassembler::Fail;

  DecodeStatus S;
  switch (Width) {
  case 16:
    S = decodeInstruction(DecoderTableForm16, MI, uint32_t(W >> 32), Address,
                          this, STI);
    break;
  case 32:
    S = decodeInstruction(DecoderTableForm32, MI, uint32_t(W >> 16), Address,
                          this, STI);
    break;
  default:
    S = decodeInstruction(DecoderTableForm48, MI, W, Address, this, STI);
    break;
  }
  if (S != MCDisassembler::Fail)
    Size = Width / 8;
  return S;
}
