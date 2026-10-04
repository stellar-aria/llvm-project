# RUN: llvm-mc -triple=sharc -mcpu=adsp-2156x -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s
#
# SHARC+ counter loops are F1-active, (F), when the last eleven instructions
# hold no branch or IDLE (PRM 4-41 text, "Loop Categorization"); E2-active
# otherwise. An explicit (F) is kept.

LCNTR = 4, DO a UNTIL LCE;
r0 = r0 + r1;
a: r0 = r0 + r1;
# CHECK: DO (PC, 2) UNTIL LCE (F);

LCNTR = 4, DO b UNTIL LCE;
JUMP (PC, 2);
nop;
b: nop;
# CHECK: DO (PC, 3) UNTIL LCE;

LCNTR = 4, DO c UNTIL LCE;
idle;
c: nop;
# CHECK: DO (PC, 2) UNTIL LCE;

## A branch more than eleven instructions before the end does not count.
LCNTR = 4, DO d UNTIL LCE;
JUMP (PC, 1);
nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;
d: nop;
# CHECK: DO (PC, 13) UNTIL LCE (F);

## A nested loop's end is a change of flow in the outer loop.
LCNTR = 4, DO outer UNTIL LCE;
LCNTR = 2, DO inner UNTIL LCE;
inner: nop;
outer: nop;
# CHECK: DO (PC, 3) UNTIL LCE;
# CHECK: DO (PC, 1) UNTIL LCE (F);

LCNTR = 4, DO e UNTIL LCE (F);
JUMP (PC, 1);
e: nop;
# CHECK: DO (PC, 2) UNTIL LCE (F);
