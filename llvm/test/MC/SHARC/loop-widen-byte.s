# RUN: llvm-mc -triple=sharc -mcpu=adsp-2156x -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s
#
# A byte load at the end of a VISA loop widens from Type 3b to Type 3d
# without (EX), the 48-bit form SHARC+ code uses there.

.text
.visa
LCNTR = 2, DO end UNTIL LCE;
R1 = DM(I1, M2) (BWSE);
end: R0 = DM(I1, M2) (BW);
R0 = DM(I1, M2) (BW);

# CHECK:      6: {{.*}} R1 = DM(I1, M2) (BWSE);
# CHECK:      c: {{.*}} R0 = DM(I1, M2) (BW);
# CHECK-NEXT: 12: {{.*}} R0 = DM(I1, M2) (BW);
