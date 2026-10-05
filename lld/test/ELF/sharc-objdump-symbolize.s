# REQUIRES: sharc
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-2156x %s -o %t.o
# RUN: ld.lld -T %S/Inputs/sharc-2156x-test.ld %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

## llvm-objdump maps absolute code and data addresses back to byte addresses
## through the block table and names their symbols.
.text
.globl _start
_start:
# ISA 0x90008 -> byte 0x240000 + 8 * 6 = 0x240030 = target2.
# CHECK: JUMP 0x90008;{{.*}}<target2>
  JUMP target2;
# NW 0xB0002 -> byte 0x2C0000 + 2 * 4 = 0x2C0008 = var.
# CHECK: R0 = DM(0xb0002);{{.*}}<var>
  R0 = DM(var);

.section .text2,"ax"
target2:
  NOP;

.section .vtext,"axv"
# SW 0x120400 -> byte 0x240000 + 0x400 * 2 = 0x240800 = vstart.
# CHECK: JUMP 0x120400;{{.*}}<vstart>
vstart:
  JUMP vstart;

.data
  .word 0
  .word 0
var:
  .word 1
