# RUN: llvm-mc -triple=sharc -mcpu=adsp-2137x -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s
# RUN: llvm-objdump -d --mcpu=adsp-2137x %t.o | FileCheck %s --check-prefix=DIS
#
# ISA code: 48-bit instructions, PC-relative offsets count instructions
# (6 bytes). Absolute code addresses are 48-bit normal-word addresses and data
# addresses normal-word addresses (SHARCUsage.rst, "Relocations").

JUMP ext_func;                      # 0x00
CALL ext_func (DB);                 # 0x06
R0 = ext_data;                      # 0x0c
I4 = DM(ext_data + 8);              # 0x12
JUMP (PC, local);                   # 0x18
NOP;                                # 0x1e
NOP;                                # 0x24
local: NOP;                         # 0x2a
LCNTR = 4, DO loopend UNTIL LCE;    # 0x30
NOP;                                # 0x36
loopend: NOP;                       # 0x3c
JUMP (PC, other);                   # 0x42
.word ext_data                      // 0x48

.section .text.b,"ax"
other: NOP;

# CHECK:      Section ({{[0-9]+}}) .rela.text {
# CHECK-NEXT:   0x0 R_SHARC_ADDR24_NW ext_func 0x0
# CHECK-NEXT:   0x6 R_SHARC_ADDR24_NW ext_func 0x0
# CHECK-NEXT:   0xC R_SHARC_ADDR32_NW ext_data 0x0
# CHECK-NEXT:   0x12 R_SHARC_ADDR32_NW ext_data 0x8
# CHECK-NEXT:   0x42 R_SHARC_PCREL24_ISA .text.b 0x0
# CHECK-NEXT:   0x48 R_SHARC_DATA32 ext_data 0x0
# CHECK-NEXT: }

# DIS:      JUMP 0x0;
# DIS:      JUMP (PC, 3);
# DIS:      LCNTR = 4, DO (PC, 2) UNTIL LCE;
# DIS:      JUMP (PC, 0);
