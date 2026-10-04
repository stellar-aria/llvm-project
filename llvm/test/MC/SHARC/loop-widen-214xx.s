# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s
#
# In VISA code the last four instructions of a hardware loop are 48-bit
# (PGR 4-54, "VISA-Related Restrictions on Hardware Loops").

.text
.visa
LCNTR = 10, DO end UNTIL LCE;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
r0 = r0 + r1;
end: r0 = r0 + r1;
r0 = r0 + r1;

# CHECK:      0: {{.*}} LCNTR = 10, DO (PC, 13) UNTIL LCE;
# CHECK-NEXT: 6: 01 c0 R0 = R0 + R1;
# CHECK-NEXT: 8: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: e: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: 14: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK:      1a: 3e 01 00 00 01 10 R0 = R0 + R1;
# CHECK-NEXT: 20: 01 c0 R0 = R0 + R1;
