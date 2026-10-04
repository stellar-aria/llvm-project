# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s
#
# A loop shorter than four instructions is entirely 48-bit (PGR 4-54).

.text
.visa
LCNTR = 3, DO end UNTIL LCE;
r0 = r0 + r1;
r0 = r0 + r1;
end: r0 = r0 + r1;
nop;

# CHECK:      0: {{.*}} DO (PC, 9) UNTIL LCE;
# CHECK-NEXT: 6: 3e 01 00 00 01 10
# CHECK-NEXT: c: 3e 01 00 00 01 10
# CHECK:      12: 3e 01 00 00 01 10
# CHECK-NEXT: 18: 01 00 NOP;
