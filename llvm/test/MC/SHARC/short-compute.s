# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -mattr=+visa-section -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -mattr=+visa-section -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --mcpu=adsp-214xx %t.o | FileCheck %s --check-prefix=DIS
#
# The short computes of Type 2c (16-bit, VISA): 1100, then the 12-bit short
# compute with its opcode at 11-8, Rn at 7-4 and Rx at 3-0 (PGR short compute
# table). Expected encodings are from digiemu's compute and form tables and
# were checked against that field layout.

# SC_0001
R1 = R1 - R2;
# CHECK: R1 = R1 - R2; // encoding: [0x12,0xc1]
# DIS: R1 = R1 - R2;

# SC_0010
R3 = PASS R4;
# CHECK: R3 = PASS R4; // encoding: [0x34,0xc2]
# DIS: R3 = PASS R4;

# SC_0011
COMP(R5, R6);
# CHECK: COMP(R5, R6); // encoding: [0x56,0xc3]
# DIS: COMP(R5, R6);

# SC_0100
R7 = NOT R8;
# CHECK: R7 = NOT R8; // encoding: [0x78,0xc4]
# DIS: R7 = NOT R8;

# SC_0101
R9 = R10 + 1;
# CHECK: R9 = R10 + 1; // encoding: [0x9a,0xc5]
# DIS: R9 = R10 + 1;

# SC_0110
R11 = R12 - 1;
# CHECK: R11 = R12 - 1; // encoding: [0xbc,0xc6]
# DIS: R11 = R12 - 1;

# SC_0111
R13 = R13 * R14 (SSI);
# CHECK: R13 = R13 * R14 (SSI); // encoding: [0xde,0xc7]
# DIS: R13 = R13 * R14 (SSI);

# SC_1000
F1 = F1 + F2;
# CHECK: F1 = F1 + F2; // encoding: [0x12,0xc8]
# DIS: F1 = F1 + F2;

# SC_1001
F3 = F3 - F4;
# CHECK: F3 = F3 - F4; // encoding: [0x34,0xc9]
# DIS: F3 = F3 - F4;

# SC_1010
F5 = FLOAT R6;
# CHECK: F5 = FLOAT R6; // encoding: [0x56,0xca]
# DIS: F5 = FLOAT R6;

# SC_1011
COMP(F7, F8);
# CHECK: COMP(F7, F8); // encoding: [0x78,0xcb]
# DIS: COMP(F7, F8);

# SC_1100
R9 = R9 AND R10;
# CHECK: R9 = R9 AND R10; // encoding: [0x9a,0xcc]
# DIS: R9 = R9 AND R10;

# SC_1101
R11 = R11 OR R12;
# CHECK: R11 = R11 OR R12; // encoding: [0xbc,0xcd]
# DIS: R11 = R11 OR R12;

# SC_1110
R13 = R13 XOR R14;
# CHECK: R13 = R13 XOR R14; // encoding: [0xde,0xce]
# DIS: R13 = R13 XOR R14;
