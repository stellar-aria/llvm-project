# REQUIRES: sharc
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-2156x %s -o %t.o
# RUN: ld.lld -T %S/Inputs/sharc-2156x-test.ld %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

# word = Wbase + (S + A - Bbase) / unit, per block (SHARCUsage.rst), with the
# bases of the ADSP-2156x data sheet, Table 2 and Table 3.

.text
.globl _start
_start:
# target2 = 0x240030: ISA 0x90000 + 0x30 / 6 = 0x90008.
# CHECK: JUMP 0x90008;
  JUMP target2;
# l2fn = 0x20000000: L2 ISA base 0x5C0000.
# CHECK: CALL 0x5c0000;
  CALL l2fn;
# var = 0x2C0008: block 1 NW 0xB0000 + 8 / 4 = 0xB0002.
# CHECK: R0 = DM(0xb0002);
  R0 = DM(var);
# l2var = 0x20000100: L2 NW 0x08000000 + 0x100 / 4 = 0x08000040.
# CHECK: DM(0x8000040) = R1;
  DM(l2var) = R1;
# CHECK: R2 = 0xb0002;
  R2 = var;
# CJUMP takes an ISA address too: target2 -> 0x90008.
# CHECK: CJUMP 0x90008 (DB);
  CJUMP target2 (DB);
  NOP;
  NOP;

.section .text2,"ax"
target2:
  NOP;

.section .vtext,"axv"
vstart:
# vtarget = 0x240806: SW 0x120000 + 0x806 / 2 = 0x120403.
# CHECK: JUMP 0x120403;
  JUMP vtarget;
vtarget:
  NOP;

.data
  .word 0
  .word 0
var:
  .word 42

.section .l2text,"ax"
l2fn:
  RTS;

.section .l2data,"aw"
l2var:
  .word 0
