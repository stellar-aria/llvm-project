# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2137x %s -o /dev/null 2>&1 | FileCheck %s
#
# Restrictions on ending loops (PGR 4-55).

LCNTR = 10, DO a UNTIL LCE;
nop;
JUMP (PC, 2);
# CHECK: :[[@LINE-1]]:1: error: JUMP or CALL in the last three instructions of a hardware loop
nop;
a: nop;

LCNTR = 10, DO b UNTIL LCE;
JUMP (PC, 4);
nop;
nop;
b: nop;
# CHECK-NOT: :[[@LINE-4]]:{{.*}}error

## IDLE in a counter loop of three or fewer instructions.
LCNTR = 10, DO c UNTIL LCE;
IDLE;
# CHECK: :[[@LINE-1]]:1: error: IDLE or EMUIDLE at this position of a hardware loop
c: nop;

## IDLE as the fifth-from-last instruction of a longer counter loop.
LCNTR = 10, DO d UNTIL LCE;
IDLE;
# CHECK: :[[@LINE-1]]:1: error: IDLE or EMUIDLE at this position of a hardware loop
nop; nop; nop;
d: nop;

## IDLE in the last three of an arithmetic loop.
DO e UNTIL EQ;
nop;
EMUIDLE;
# CHECK: :[[@LINE-1]]:1: error: IDLE or EMUIDLE at this position of a hardware loop
e: nop;

## Loading CURLCNTR at e-4 of a counter loop.
LCNTR = 10, DO f UNTIL LCE;
CURLCNTR = DM(I0, M0);
# CHECK: :[[@LINE-1]]:1: error: load of CURLCNTR at the fifth-from-last instruction of a counter loop
nop; nop; nop;
f: nop;

## IF NOT LCE right after a write to CURLCNTR.
CURLCNTR = 5;
IF NOT LCE R0 = R1 + R2;
# CHECK: :[[@LINE-1]]:1: error: IF NOT LCE after a write to CURLCNTR
