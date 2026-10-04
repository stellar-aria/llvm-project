# RUN: split-file %s %t
# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2156x -filetype=obj %t/narrow.s -o /dev/null 2>&1 \
# RUN:   | FileCheck %s --check-prefix=NARROW
# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2156x -filetype=obj %t/open.s -o /dev/null 2>&1 \
# RUN:   | FileCheck %s --check-prefix=OPEN

#--- narrow.s
## A byte load has no 48-bit form without (EX), so it cannot end a loop in
## VISA code.
.text
.visa
LCNTR = 2, DO end UNTIL LCE;
end: R0 = DM(I1, M2) (BW);
# NARROW: narrow.s:6:6: error: no 48-bit form of this instruction for the end of a hardware loop

#--- open.s
LCNTR = 2, DO nowhere UNTIL LCE;
# OPEN: open.s:1:15: error: hardware loop end 'nowhere' is not defined in this section
