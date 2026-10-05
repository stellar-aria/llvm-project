# REQUIRES: sharc
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-2156x %s -o %t.o
# RUN: not ld.lld -T %S/Inputs/sharc-2156x-test.ld %t.o -o /dev/null \
# RUN:   --defsym=gap=0x100000 --defsym=odd_code=0x240002 --defsym=odd_data=0x2C0001 2>&1 \
# RUN:   | FileCheck %s

.text
.globl _start
_start:
## Not in any block: 0x100000 lies below L1 block 0.
# CHECK-DAG: error: {{.*}}R_SHARC_ADDR32_NW against gap: address 0x100000 is outside the memory blocks of adsp-2156x
  R0 = DM(gap);
## Misaligned: 0x240002 is not on a 48-bit row of l1-block0.
# CHECK-DAG: error: {{.*}}R_SHARC_ADDR24_NW against odd_code: address 0x240002 is not a 48-bit instruction address in l1-block0
  JUMP odd_code;
# CHECK-DAG: error: {{.*}}R_SHARC_ADDR32_NW against odd_data: address 0x2C0001 is not a normal-word address in l1-block1
  R1 = DM(odd_data);
## Out of range: Type 10a's 6-bit offset. The instruction is at 0x240012
## (three 48-bit instructions before it) and far at 0x240600:
## 0x5EE / 6 = 253 instructions > 31.
# CHECK-DAG: error: {{.*}}relocation R_SHARC_PCREL6_ISA out of range: 253 is not in [-32, 31]
  IF EQ JUMP (PC, far), ELSE R0 = DM(I0, M1);
## PC-relative across blocks: l2fn is in L2. The assembler relocates against
## the local label's section.
# CHECK-DAG: error: {{.*}}R_SHARC_PCREL24_ISA against .l2text: branch crosses from l1-block0 to l2
  JUMP (PC, l2fn);

.section .text3,"ax"
far:
  NOP;

.section .l2text,"ax"
l2fn:
  RTS;
