# REQUIRES: sharc
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-2156x %s -o %t.o

## The VISA flag survives on output sections (SHF_SHARC_VISA, shown as v).
# RUN: ld.lld -T %S/Inputs/sharc-2156x-test.ld %t.o -o %t
# RUN: llvm-readelf -S %t | FileCheck %s --check-prefix=FLAGS
# FLAGS: .vtext PROGBITS 00240800 {{[0-9a-f]+}} {{[0-9a-f]+}} 00 AXv

## ISA code must start on a 48-bit row: 0x240004 is 4 bytes past block 0's
## ISA base, not a multiple of 6.
# RUN: echo 'SECTIONS { .text 0x240004 : { *(.text) } .vtext 0x240800 : { *(.vtext) } }' > %t.bad.ld
# RUN: not ld.lld -T %t.bad.ld %t.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=ALIGN
# ALIGN: error: section .text holds 48-bit code but starts at 0x240004, which is not a 48-bit instruction address of l1-block0; place it with a linker script

## VISA and ISA input sections in one output section.
# RUN: echo 'SECTIONS { .text 0x240000 : { *(.text) *(.vtext) } }' > %t.mix.ld
# RUN: not ld.lld -T %t.mix.ld %t.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=MIX
# MIX: error: output section .text mixes VISA (.vtext) and ISA code

## Without a script, the image starts at L1 block 0 (byte 0x240000). Only
## VISA code can be placed this way: ISA code after the ELF headers would not
## start on a 48-bit row.
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-2156x --defsym=VISAONLY=1 %s -o %t.visa.o
# RUN: ld.lld %t.visa.o -o %t.default
# RUN: llvm-readelf -S %t.default | FileCheck %s --check-prefix=DEFAULT
# DEFAULT: .vtext PROGBITS 0024{{[0-9a-f]+}}

.globl _start
.ifndef VISAONLY
.text
_start:
  NOP;
.endif
.section .vtext,"axv"
.ifdef VISAONLY
_start:
.endif
  NOP;
