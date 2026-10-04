# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -mattr=+visa-section -show-encoding %s \
# RUN:   | FileCheck %s
# Short data fields are sign-extended (SHARCUsage.rst, "Immediate data"), so
# the 32-bit forms take only values they hold exactly; others use 48 bits.

R0 = -1;
# CHECK: R0 = -0x1;{{ +}}// encoding: [0x80,0x0f,0xff,0xff]
R0 = 0x7fff;
# CHECK: R0 = 0x7fff;{{ +}}// encoding: [0x80,0x0f,0xff,0x7f]
R0 = 0xffff;
# CHECK: R0 = 0xffff;{{ +}}// encoding: [0x00,0x0f,0x00,0x00,0xff,0xff]
R0 = -0x8000;
# CHECK: R0 = -0x8000;{{ +}}// encoding: [0x80,0x0f,0x00,0x80]
R0 = -0x8001;
# CHECK: R0 = 0xffff7fff;{{ +}}// encoding: [0x00,0x0f,0xff,0xff,0xff,0x7f]
