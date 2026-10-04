//===-- SHARCRegisterInfo.td - SHARC register definitions --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// HWEncoding is the 7-bit universal register (Ureg) code: bits 6-4 select the
// group, bits 3-0 the register (PGR Table 10-2, p473).
//
//===----------------------------------------------------------------------===//

let Namespace = "SHARC" in {
def FName : RegAltNameIndex;

def sub_even : SubRegIndex<32>;
def sub_odd : SubRegIndex<32, 32>;
}

class SHARCReg<bits<7> enc, string n, list<string> alt = []>
    : Register<n, alt> {
  let Namespace = "SHARC";
  let HWEncoding{6-0} = enc;
  let RegAltNameIndices = !if(!empty(alt), [], [FName]);
}

// Data registers: R0-R15 read as fixed point, F0-F15 as floating point.
// S0-S15 are the PEy data registers (SF0-SF15).
foreach i = 0-15 in {
  def R#i : SHARCReg<!add(0x00, i), "R"#i, ["F"#i]>;
  def I#i : SHARCReg<!add(0x10, i), "I"#i>;
  def M#i : SHARCReg<!add(0x20, i), "M"#i>;
  def L#i : SHARCReg<!add(0x30, i), "L"#i>;
  def B#i : SHARCReg<!add(0x40, i), "B"#i>;
  def S#i : SHARCReg<!add(0x50, i), "S"#i, ["SF"#i]>;
}

// Group 110: sequencer, emulation, PX and timer registers. Code 0x62 is
// reserved.
def FADDR    : SHARCReg<0x60, "FADDR">;
def DADDR    : SHARCReg<0x61, "DADDR">;
def PC       : SHARCReg<0x63, "PC">;
def PCSTK    : SHARCReg<0x64, "PCSTK">;
def PCSTKP   : SHARCReg<0x65, "PCSTKP">;
def LADDR    : SHARCReg<0x66, "LADDR">;
def CURLCNTR : SHARCReg<0x67, "CURLCNTR">;
def LCNTR    : SHARCReg<0x68, "LCNTR">;
def EMUCLK   : SHARCReg<0x69, "EMUCLK">;
def EMUCLK2  : SHARCReg<0x6A, "EMUCLK2">;
def PX       : SHARCReg<0x6B, "PX">;
def PX1      : SHARCReg<0x6C, "PX1">;
def PX2      : SHARCReg<0x6D, "PX2">;
def TPERIOD  : SHARCReg<0x6E, "TPERIOD">;
def TCOUNT   : SHARCReg<0x6F, "TCOUNT">;

// Group 111: system registers. The PGR prints 0x7D as "LRPTL"; the register
// is LIRPTL. SHARC+ replaces it with MODE1STK (PRM p551).
def USTAT1   : SHARCReg<0x70, "USTAT1">;
def USTAT2   : SHARCReg<0x71, "USTAT2">;
def MODE1    : SHARCReg<0x72, "MODE1">;
def MMASK    : SHARCReg<0x73, "MMASK">;
def MODE2    : SHARCReg<0x74, "MODE2">;
def FLAGS    : SHARCReg<0x75, "FLAGS">;
def ASTATX   : SHARCReg<0x76, "ASTATX">;
def ASTATY   : SHARCReg<0x77, "ASTATY">;
def STKYX    : SHARCReg<0x78, "STKYX">;
def STKYY    : SHARCReg<0x79, "STKYY">;
def IRPTL    : SHARCReg<0x7A, "IRPTL">;
def IMASK    : SHARCReg<0x7B, "IMASK">;
def IMASKP   : SHARCReg<0x7C, "IMASKP">;
def LIRPTL   : SHARCReg<0x7D, "LIRPTL">;
def USTAT3   : SHARCReg<0x7E, "USTAT3">;
def USTAT4   : SHARCReg<0x7F, "USTAT4">;
def MODE1STK : SHARCReg<0x7D, "MODE1STK">;

//===----------------------------------------------------------------------===//
// Register classes
//===----------------------------------------------------------------------===//

class SHARCRegClass<list<ValueType> types, dag regs>
    : RegisterClass<"SHARC", types, 32, regs>;

def RFREG : SHARCRegClass<[i32, f32], (sequence "R%u", 0, 15)>;
def SREG  : SHARCRegClass<[i32, f32], (sequence "S%u", 0, 15)>;
def IREG  : SHARCRegClass<[i32], (sequence "I%u", 0, 15)>;
def MREG  : SHARCRegClass<[i32], (sequence "M%u", 0, 15)>;
def LREG  : SHARCRegClass<[i32], (sequence "L%u", 0, 15)>;
def BREG  : SHARCRegClass<[i32], (sequence "B%u", 0, 15)>;

// DAG1 (I0-I7, M0-M7) and DAG2 (I8-I15, M8-M15) halves.
def I1REG : SHARCRegClass<[i32], (sequence "I%u", 0, 7)>;
def I2REG : SHARCRegClass<[i32], (sequence "I%u", 8, 15)>;
def M1REG : SHARCRegClass<[i32], (sequence "M%u", 0, 7)>;
def M2REG : SHARCRegClass<[i32], (sequence "M%u", 8, 15)>;
def B1REG : SHARCRegClass<[i32], (sequence "B%u", 0, 7)>;
def B2REG : SHARCRegClass<[i32], (sequence "B%u", 8, 15)>;

// Restricted 2-bit input operands of multifunction computations
// (PRM Tables 18-15 to 18-19).
def RXMREG : SHARCRegClass<[i32, f32], (sequence "R%u", 0, 3)>;
def RYMREG : SHARCRegClass<[i32, f32], (sequence "R%u", 4, 7)>;
def RXAREG : SHARCRegClass<[i32, f32], (sequence "R%u", 8, 11)>;
def RYAREG : SHARCRegClass<[i32, f32], (sequence "R%u", 12, 15)>;

def MISCREG : SHARCRegClass<[i32],
    (add FADDR, DADDR, PC, PCSTK, PCSTKP, LADDR, CURLCNTR, LCNTR, EMUCLK,
         EMUCLK2, PX, PX1, PX2, TPERIOD, TCOUNT)>;
def SYSREG : SHARCRegClass<[i32],
    (add USTAT1, USTAT2, MODE1, MMASK, MODE2, FLAGS, ASTATX, ASTATY, STKYX,
         STKYY, IRPTL, IMASK, IMASKP, LIRPTL, USTAT3, USTAT4)>;

// Universal registers, in Ureg code order. MODE1STK (SHARC+) shares
// LIRPTL's code and is selected by subtarget in the printer and parser.
def UREG : SHARCRegClass<[i32],
    (add RFREG, IREG, MREG, LREG, BREG, SREG, MISCREG, SYSREG)>;

// 64-bit register pairs F1:0 ... F15:14 (SHARC+, PRM Table 18-28).
def DBLPAIRS : RegisterTuples<[sub_even, sub_odd],
                              [(decimate (sequence "R%u", 0, 15), 2),
                               (decimate (sequence "R%u", 1, 15), 2)]>;
def DBLREG : RegisterClass<"SHARC", [f64], 64, (add DBLPAIRS)>;
