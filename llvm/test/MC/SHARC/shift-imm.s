# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --mcpu=adsp-214xx %t.o | FileCheck %s --check-prefix=DIS
#
# Shift immediates whose 12-bit immediate is DATAEX:DATA (PGR Table 10-1),
# and the bit operations with <data8>. Syntax and shift opcodes are from PGR
# Table 12-11; the shift-immediate opcode is the upper six bits of the shift
# opcode (PGR Table 12-2). Encodings are computed from those fields.

# FEXT, PGR Table 12-11 0100 0000
R0 = FEXT R1 BY 8:16;
# CHECK: R0 = FEXT R1 BY 8:16; // encoding: [0x3e,0x80,0x10,0x20,0x01,0x08]
# DIS: R0 = FEXT R1 BY 8:16;

# FDEP (SE)
R2 = FDEP R3 BY 4:8 (SE);
# CHECK: R2 = FDEP R3 BY 4:8 (SE); // encoding: [0x3e,0x80,0x13,0x10,0x23,0x04]
# DIS: R2 = FDEP R3 BY 4:8 (SE);

# OR FDEP: shift opcode 0110 0100, so 011001 (not 011011 as in the encoding table)
R4 = R4 OR FDEP R5 BY 0:31;
# CHECK: R4 = R4 OR FDEP R5 BY 0:31; // encoding: [0x3e,0x80,0x19,0x38,0x45,0xc0]
# DIS: R4 = R4 OR FDEP R5 BY 0:31;

# OR FDEP (SE): 0110 1100
R4 = R4 OR FDEP R5 BY 1:2 (SE);
# CHECK: R4 = R4 OR FDEP R5 BY 1:2 (SE); // encoding: [0x3e,0x80,0x1b,0x00,0x45,0x81]
# DIS: R4 = R4 OR FDEP R5 BY 1:2 (SE);

# FEXT (SE) with a condition
IF NE R0 = FEXT R1 BY 3:4 (SE);
# CHECK: IF NE R0 = FEXT R1 BY 3:4 (SE); // encoding: [0x20,0x80,0x12,0x08,0x01,0x03]
# DIS: IF NE R0 = FEXT R1 BY 3:4 (SE);

# FDEP
R2 = FDEP R3 BY 0:1;
# CHECK: R2 = FDEP R3 BY 0:1; // encoding: [0x3e,0x80,0x11,0x00,0x23,0x40]
# DIS: R2 = FDEP R3 BY 0:1;

# BITEXT, 12-bit length
R6 = BITEXT 12;
# CHECK: R6 = BITEXT 12; // encoding: [0x3e,0x80,0x14,0x00,0x60,0x0c]
# DIS: R6 = BITEXT 12;

# BITEXT (NU): 0101 1000, so 010110 (not 011001)
R6 = BITEXT 4095 (NU);
# CHECK: R6 = BITEXT 4095 (NU); // encoding: [0x3e,0x80,0x16,0x78,0x60,0xff]
# DIS: R6 = BITEXT 4095 (NU);

# BITDEP: 0111 0100, missing from the encoding table
BITDEP R7 BY 100;
# CHECK: BITDEP R7 BY 100; // encoding: [0x3e,0x80,0x1d,0x00,0x07,0x64]
# DIS: BITDEP R7 BY 100;

# BFFWRP = <data7>
BFFWRP = 33;
# CHECK: BFFWRP = 33; // encoding: [0x3e,0x80,0x1f,0x00,0x00,0x21]
# DIS: BFFWRP = 33;

# BSET with <data8> (the table gives <bitlen12>)
R1 = BSET R2 BY 31;
# CHECK: R1 = BSET R2 BY 31; // encoding: [0x3e,0x80,0x30,0x00,0x12,0x1f]
# DIS: R1 = BSET R2 BY 31;

# BCLR with <data8> (the table gives <data7>)
R1 = BCLR R2 BY 0;
# CHECK: R1 = BCLR R2 BY 0; // encoding: [0x3e,0x80,0x31,0x00,0x12,0x00]
# DIS: R1 = BCLR R2 BY 0;

# BTGL with <data8>
R1 = BTGL R2 BY 5;
# CHECK: R1 = BTGL R2 BY 5; // encoding: [0x3e,0x80,0x32,0x00,0x12,0x05]
# DIS: R1 = BTGL R2 BY 5;

# BTST with <data8>
BTST R2 BY 7;
# CHECK: BTST R2 BY 7; // encoding: [0x3e,0x80,0x33,0x00,0x02,0x07]
# DIS: BTST R2 BY 7;

# FEXT with a memory move
R0 = FEXT R1 BY 8:16, R5 = DM(I1, M2);
# CHECK: R0 = FEXT R1 BY 8:16, R5 = DM(I1, M2); // encoding: [0xbe,0x82,0x90,0x22,0x01,0x08]
# DIS: R0 = FEXT R1 BY 8:16, R5 = DM(I1, M2);

# BITDEP register form, shift opcode 0111 0100
BITDEP R7 BY R8;
# CHECK: BITDEP R7 BY R8; // encoding: [0x3e,0x01,0x27,0x00,0x78,0x40]
# DIS: BITDEP R7 BY R8;

# ROT: shift opcode 0000 1000, so 000010
R1 = ROT R2 BY 5;
# CHECK: R1 = ROT R2 BY 5; // encoding: [0x3e,0x80,0x02,0x00,0x12,0x05]
# DIS: R1 = ROT R2 BY 5;

# A negative count sets DATAEX to all ones.
R1 = ROT R2 BY -3;
# CHECK: R1 = ROT R2 BY -3; // encoding: [0x3e,0x80,0x02,0x78,0x12,0xfd]
# DIS: R1 = ROT R2 BY -3;

# OR LSHIFT: 0010 0000
R3 = R3 OR LSHIFT R4 BY 7;
# CHECK: R3 = R3 OR LSHIFT R4 BY 7; // encoding: [0x3e,0x80,0x08,0x00,0x34,0x07]
# DIS: R3 = R3 OR LSHIFT R4 BY 7;

# OR ASHIFT: 0010 0100
R5 = R5 OR ASHIFT R6 BY -2;
# CHECK: R5 = R5 OR ASHIFT R6 BY -2; // encoding: [0x3e,0x80,0x09,0x78,0x56,0xfe]
# DIS: R5 = R5 OR ASHIFT R6 BY -2;
