# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -mattr=+visa-section -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s
# RUN: llvm-objdump -d --mcpu=adsp-214xx --mattr=+visa-section %t.o \
# RUN:   | FileCheck %s --check-prefix=DIS
#
# VISA code: PC-relative offsets count 16-bit parcels and code addresses are
# short-word addresses.

JUMP ext_func;                      # 0x00, 48-bit
JUMP (PC, local);                   # 0x06, 32-bit Type 9b: local is in range
R0 = ext_data;                      # 0x0a, 48-bit: no 16-bit data for a symbol
NOP;                                # 0x10, 16-bit
R0 = R1 + R2;                       # 0x12, 32-bit
local: NOP;                         # 0x16
JUMP (PC, other);                   # 0x18, relaxed to 48 bits: other section

.section .text.b,"ax"
other: NOP;

# CHECK:      Section ({{[0-9]+}}) .rela.text {
# CHECK-NEXT:   0x0 R_SHARC_ADDR24_SW ext_func 0x0
# CHECK-NEXT:   0xA R_SHARC_ADDR32_NW ext_data 0x0
# CHECK-NEXT:   0x18 R_SHARC_PCREL24_VISA .text.b 0x0
# CHECK-NEXT: }

# (0x16 - 0x06) / 2 = 8 parcels.
# DIS: JUMP (PC, 8);
