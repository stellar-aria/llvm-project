# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2137x %s -o /dev/null 2>&1 | FileCheck %s
#
# The two instructions after a delayed branch execute before it takes effect;
# some operations are errors there (PGR 4-23, "Restrictions when Using
# Delayed Branches").

JUMP (PC, 10) (DB);
nop;
JUMP (PC, 4);
# CHECK: :[[@LINE-1]]:1: error: branch in the delay slot of a delayed branch

CALL (PC, 10) (DB);
RTS;
# CHECK: :[[@LINE-1]]:1: error: branch in the delay slot of a delayed branch
nop;

## Mutually exclusive conditions are allowed.
IF GT JUMP (PC, 7) (DB);
IF LE JUMP (PC, 11) (DB);
nop;
# CHECK-NOT: :[[@LINE-3]]:{{.*}}error
nop;

IF EQ JUMP (PC, 7) (DB);
IF NE JUMP (PC, 4);
nop;
# CHECK-NOT: :[[@LINE-2]]:{{.*}}error

CALL (PC, 5) (DB);
PUSH PCSTK;
# CHECK: :[[@LINE-1]]:1: error: PC stack push or pop in the delay slot of a delayed branch
nop;

JUMP (PC, 5) (DB);
nop;
PCSTK = R0;
# CHECK: :[[@LINE-1]]:1: error: write to PCSTK or PCSTKP in the delay slot of a delayed branch

RTS (DB);
IDLE;
# CHECK: :[[@LINE-1]]:1: error: IDLE in the delay slot of a delayed branch
nop;

## Two slots only.
JUMP (PC, 9) (DB);
nop;
nop;
JUMP (PC, 4);
# CHECK-NOT: :[[@LINE-1]]:{{.*}}error
