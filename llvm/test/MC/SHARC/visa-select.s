# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -mattr=+visa-section -show-encoding %s | FileCheck %s
# Expected encodings are from digiemu's encoding tables.

# 16-bit Type 2c when RN = RN op RX
r0 = r0 + r1;
# CHECK: encoding: [0x01,0xc0]

# 32-bit Type 2b otherwise
r0 = r1 + r2;
# CHECK: encoding: [0x80,0x01,0x12,0x10]

# 48-bit Type 2a with a condition
IF EQ r0 = r1 + r2;
# CHECK: encoding: [0x00,0x01,0x00,0x00,0x12,0x10]

# 16-bit NOP
nop;
# CHECK: encoding: [0x01,0x00]

# 16-bit DM move
DM(I3, M7) = R11;
# CHECK: encoding: [0xfb,0x97]

# 16-bit return
IF EQ RTS (DB);
# CHECK: encoding: [0xc0,0x0a]
