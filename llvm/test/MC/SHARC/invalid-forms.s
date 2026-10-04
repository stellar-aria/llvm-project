# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2137x %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=sharc -mcpu=adsp-214xx -mattr=+visa-section %s 2>&1 \
# RUN:   | FileCheck %s --check-prefix=VISA

## Three memory moves: no form has them.
R0 = R1 + R2, DM(I0,M0) = R3, PM(I8,M8) = R4, DM(I1,M1) = R5;
# CHECK: :[[@LINE-1]]:1: error: invalid SHARC instruction

## SHARC+ forms on a classic core.
SYNC;
# CHECK: :[[@LINE-1]]:1: error: instruction requires: sharcplus
R4 = DM(0x1000) (EX);
# CHECK: :[[@LINE-1]]:1: error: instruction requires: sharcplus

## 64-bit floating point needs SHARC+.
F3:2 = F5:4 + F9:8;
# CHECK: :[[@LINE-1]]:1: error: instruction requires: fp64

## Type 10 is not valid VISA (PGR p226).
IF NE JUMP (PC, -32), ELSE R6 = DM(I5, M0);
# VISA: :[[@LINE-1]]:1: error: Type 10 instructions are not available in VISA code

## A 6-bit PC-relative offset out of range.
IF NE JUMP (PC, 32), ELSE R6 = DM(I5, M0);
# CHECK: :[[@LINE-1]]:1: error: invalid SHARC instruction

## BITEXT, BITDEP and BFFWRP are ADSP-214xx and later (PGR Table 12-11).
R0 = BITEXT R1;
# CHECK: :[[@LINE-1]]:1: error: instruction requires: visa
BITDEP R7 BY 100;
# CHECK: :[[@LINE-1]]:1: error: instruction requires: visa
