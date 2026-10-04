# RUN: llvm-mc -triple=sharc -mcpu=adsp-2137x -show-encoding %s | FileCheck %s
# Expected encodings are from digiemu's encoding tables.

# PGR 9-7 Type 1 example, written as in the manual
R8=DM(I4,M1), PM(I12,M12)=R0;
# CHECK: encoding: [0x71,0x28,0x00,0x20,0x00,0x00]

# PGR 9-12 Type 3 example
R6=R3-R11, DM(I0,M1)=ASTATx;
# CHECK: encoding: [0x7e,0x50,0x00,0xbb,0x3b,0x26]

# PGR 9-12 Type 3 example: a Ureg data register may be written as F
IF NOT SV F8=CLIP F2 BY F14, F7=PM(I12,M12);
# CHECK: encoding: [0x2f,0x59,0x8e,0x03,0x2e,0x38]

# PGR 9-10 Type 2 example
IF MV R6=SAT MRF (UI);
# CHECK: encoding: [0x0a,0x01,0x10,0x00,0x00,0x06]

# PGR 9-32 Type 8 example
IF AV JUMP(PC,0x00A4)(LA);
# CHECK: encoding: [0x48,0x07,0x00,0x00,0xa4,0x00]

# PGR 9-32
JUMP (PC,2)(DB,CI);
# CHECK: encoding: [0x3e,0x07,0x00,0x05,0x02,0x00]
nop; nop;  // the two delay slots

# PGR 9-35 Type 9 example
IF EQ CALL(PC,17)(DB), ELSE R6=R6-1;
# CHECK: encoding: [0x80,0x09,0x02,0x8e,0x60,0xa6]
nop; nop;  // the two delay slots

# PGR 9-40 Type 10 example, with the 6-bit offset written as -32 (the manual writes 0x20)
IF NE JUMP(PC,-32), ELSE F12=FLOAT R10 BY R3, R6=DM(I5,M0);
# CHECK: encoding: [0x21,0xea,0x0d,0x03,0xa3,0xac]

# Type 11
RTS(DB);
# CHECK: encoding: [0x3e,0x0a,0x00,0x04,0x00,0x00]
nop; nop;  // the two delay slots

# Type 12, numeric PC-relative end
LCNTR=10, DO(PC,5)UNTIL LCE;
# CHECK: encoding: [0x00,0x0c,0x00,0x0a,0x05,0x00]

# Type 7 with a destination register
I3=MODIFY(I1,M2);
# CHECK: encoding: [0x3e,0x04,0x00,0x52,0x00,0x00]

# Type 18
BIT SET MODE1 0x1000;
# CHECK: encoding: [0x02,0x14,0x00,0x00,0x00,0x10]

# Type 18, system register written ASTATx
bit tst astatx 4;
# CHECK: encoding: [0x86,0x14,0x00,0x00,0x04,0x00]

# Type 20
PUSH LOOP, PUSH STS;
# CHECK: encoding: [0xa0,0x17,0x00,0x00,0x00,0x00]

# Type 5 swap
R0<->S0;
# CHECK: encoding: [0x3e,0x78,0x00,0x00,0x00,0x00]

# Type 6 shift immediate
R1=LSHIFT R2 BY 4;
# CHECK: encoding: [0x3e,0x80,0x00,0x00,0x12,0x04]

# Type 6 shift immediate with a memory move
R1=ASHIFT R2 BY -3, R5=DM(I1,M2);
# CHECK: encoding: [0xbe,0x82,0x81,0x02,0x12,0xfd]
