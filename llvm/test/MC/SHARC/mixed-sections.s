# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s

.section .text.isa,"ax"
JUMP f;
JUMP (PC, g);
.section .text.visa,"axv"
JUMP f;
JUMP (PC, g);

# CHECK:      .rela.text.isa {
# CHECK-NEXT:   0x0 R_SHARC_ADDR24_NW f 0x0
# CHECK-NEXT:   0x6 R_SHARC_PCREL24_ISA g 0x0
# CHECK:      .rela.text.visa {
# CHECK-NEXT:   0x0 R_SHARC_ADDR24_SW f 0x0
# CHECK-NEXT:   0x6 R_SHARC_PCREL24_VISA g 0x0
