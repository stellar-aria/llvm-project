# REQUIRES: sharc
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-2156x %s -o %t.plus.o
# RUN: ld.lld -T %S/../../../llvm/utils/sharc/ldscripts/adsp-2156x.ld %t.plus.o -o %t.plus
# RUN: llvm-readelf -S -s %t.plus | FileCheck %s --check-prefix=PLUS
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-21371 --defsym=CLASSIC=1 %s -o %t.21371.o
# RUN: ld.lld -T %S/../../../llvm/utils/sharc/ldscripts/adsp-21371.ld %t.21371.o -o %t.21371
# RUN: llvm-readelf -S -s %t.21371 | FileCheck %s --check-prefix=P21371
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-21375 --defsym=CLASSIC=1 %s -o %t.21375.o
# RUN: ld.lld -T %S/../../../llvm/utils/sharc/ldscripts/adsp-21375.ld %t.21375.o -o %t.21375
# RUN: llvm-readelf -S -s %t.21375 | FileCheck %s --check-prefix=P21375

## adsp-2156x (data sheet Table 2, Table 3): code in L1 block 0 (0x240000),
## L2 code at 0x20000000, data in block 1 (0x2C0000), stack top at the end of
## block 1: 0x2C0000 + 0x30000 = 0x2F0000.
# PLUS: .text PROGBITS 00240000
# PLUS: .l2text PROGBITS 20000000
# PLUS: .data PROGBITS 002c0000
# PLUS: 002f0000 {{.*}} __stack_end

## adsp-21371 (Table 3): block 0 RAM 0x260000, block 1 RAM 0x2E0000, stack
## top 0x2E0000 + 0x8000 = 0x2E8000.
# P21371: .text PROGBITS 00260000
# P21371: .data PROGBITS 002e0000
# P21371: 002e8000 {{.*}} __stack_end

## adsp-21375 (Table 4): the same bases, stack top 0x2E0000 + 0x4000 = 0x2E4000.
# P21375: .text PROGBITS 00260000
# P21375: .data PROGBITS 002e0000
# P21375: 002e4000 {{.*}} __stack_end

.text
.globl _start
_start:
  NOP;
  NOP;
.ifndef CLASSIC
.section .vtext,"axv"
  NOP;
.section .l2text,"axv"
  NOP;
.endif
.data
  .word 1
