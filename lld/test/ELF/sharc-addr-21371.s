# REQUIRES: sharc
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-21371 %s -o %t.o
# RUN: ld.lld -T %S/Inputs/sharc-21371-test.ld %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# RUN: llvm-objdump -s -j .data %t | FileCheck %s --check-prefix=DATA

.text
.globl _start
_start:
# target2 = 0x260030: block 0 RAM ISA 0x90000 + 0x30 / 6 = 0x90008.
# CHECK: JUMP 0x90008;
  JUMP target2;
# var = 0x2E0008: block 1 RAM NW 0xB8000 + 8 / 4 = 0xB8002 (the ISA and NW
# bases differ on this processor).
# CHECK: R0 = DM(0xb8002);
  R0 = DM(var);
# CHECK: R2 = 0xb8002;
  R2 = var;
# At 0x260012, target2 at 0x260030: (0x30 - 0x12) / 6 = 5.
# CHECK: JUMP (PC, 5);
  JUMP (PC, target2);
# Type 10a's 6-bit offset at 0x260018: (0x30 - 0x18) / 6 = 4.
# CHECK: IF EQ JUMP (PC, 4), ELSE R0 = DM(I0, M1);
  IF EQ JUMP (PC, target2), ELSE R0 = DM(I0, M1);

.section .text2,"ax"
target2:
  NOP;

.data
# R_SHARC_DATA32 is the byte address, 2 * SW on classic SHARC: 0x2E0008.
# DATA: 2e0000 08002e00 00000000 07000000
  .word var
  .word 0
var:
  .word 7
