# REQUIRES: sharc
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-21371 %s -o %t.o
# RUN: ld.lld -T %S/Inputs/sharc-21371-test.ld %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

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

.section .text2,"ax"
target2:
  NOP;

.data
  .word 0
  .word 0
var:
  .word 7
