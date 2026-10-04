# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -mattr=+visa-section -show-encoding %s \
# RUN:   | FileCheck %s
#
# -mattr=+visa-section makes VISA the default for code sections; .isa opts a
# section out and .visa back in.

.text
r0 = r0 + r1;
# CHECK: encoding: [0x01,0xc0]
.section .text.isa,"ax"
.isa
r0 = r0 + r1;
# CHECK: encoding: [0x3e,0x01,0x00,0x00,0x01,0x10]
.text
r0 = r0 + r1;
# CHECK: encoding: [0x01,0xc0]
.section .text.isa,"ax"
r0 = r0 + r1;
# CHECK: encoding: [0x3e,0x01,0x00,0x00,0x01,0x10]
.word 0x12345678
# CHECK: .word 305419896
