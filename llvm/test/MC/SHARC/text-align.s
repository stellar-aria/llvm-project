# RUN: llvm-mc -triple=sharc -mcpu=adsp-2156x -filetype=obj %s -o - | llvm-readelf -S - | FileCheck %s

## 48-bit code is 6 bytes per instruction. A code section's alignment must
## divide 6, so that sections from several objects concatenated by the linker
## keep every instruction on a 48-bit row (SHARCUsage.rst, "Linking"); 2 is
## also the parcel size.
# CHECK: .text PROGBITS {{[0-9a-f]+}} {{[0-9a-f]+}} 000006 00 AX 0 0 2

.text
  NOP;
