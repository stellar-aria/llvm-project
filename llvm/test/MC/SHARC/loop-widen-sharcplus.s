# RUN: llvm-mc -triple=sharc -mcpu=adsp-2156x -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s
#
# On SHARC+ the last eleven instructions of a loop are 48-bit (PRM p6619 text,
# 4-41), and a loop with no branch or IDLE there is F1-active, (F).

.text
.visa
LCNTR = 10, DO end UNTIL LCE;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
end: r0 = r0 + r1;

# CHECK:      0: {{.*}} LCNTR = 10, DO (PC, 35) UNTIL LCE (F);
# CHECK-NEXT: 6: 01 c0 R0 = R0 + R1;
# CHECK-NEXT: 8: 01 c0 R0 = R0 + R1;
# CHECK-NEXT: a: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: 10: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: 16: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: 1c: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: 22: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: 28: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: 2e: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: 34: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: 3a: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: 40: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK:      46: 3e 01 00 00 01 10 R0 = R0 + R1;
