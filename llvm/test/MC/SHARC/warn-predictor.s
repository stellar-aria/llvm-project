# RUN: llvm-mc -triple=sharc -mcpu=adsp-2156x -sharc-predictor-warnings %s -o /dev/null 2>&1 \
# RUN:   | FileCheck %s
# RUN: llvm-mc -triple=sharc -mcpu=adsp-2156x %s -o /dev/null 2>&1 \
# RUN:   | FileCheck %s --check-prefix=QUIET --allow-empty
#
# SHARC+ branch predictor guidance (PRM 4-12, "Restrictions Related to the
# Branch Predictor"), as opt-in warnings: twelve 48-bit instructions' worth
# of code after a branch, and in VISA code no two branches in one 48-bit
# window.

.text
.visa
JUMP (PC, 2);
JUMP (PC, 2);
# CHECK: :[[@LINE-1]]:1: warning: two branches in one 48-bit fetch window
.rept 12
IF EQ R0 = R1 + R2;
.endr

.section .text.isa,"ax"
JUMP (PC, 2);
nop;
# CHECK: :[[@LINE-2]]:1: warning: fewer than twelve 48-bit instructions follow this branch
# CHECK-NOT: warning

# QUIET-NOT: warning
