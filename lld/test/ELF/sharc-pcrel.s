# REQUIRES: sharc
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-2156x %s -o %t.o
# RUN: ld.lld -T %S/Inputs/sharc-2156x-test.ld %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# RUN: llvm-objdump -s -j .data %t | FileCheck %s --check-prefix=DATA

# PC-relative offsets count 48-bit instructions in ISA code and 16-bit
# parcels in VISA code: (S + A - P) / 6 or / 2.

.text
.globl _start
_start:
# At 0x240000, target2 at 0x240030: (0x30 - 0) / 6 = 8.
# CHECK: JUMP (PC, 8);
  JUMP (PC, target2);

.section .text2,"ax"
target2:
  NOP;
# At 0x240036, back to _start at 0x240000: (0 - 0x36) / 6 = -9.
# CHECK: JUMP (PC, -9);
  JUMP (PC, _start);

.section .vtext,"axv"
# At 0x240800, vfar at 0x240840: 0x40 / 2 = 32.
# CHECK: JUMP (PC, 32);
  JUMP (PC, vfar);

.section .vtext2,"axv"
vfar:
  NOP;

.data
# R_SHARC_DATA32 is S + A, the byte address: var at 0x2C0008.
  .word var
  .word 0
var:
  .word 42
# DATA: 2c0000 08002c00 00000000 2a000000
