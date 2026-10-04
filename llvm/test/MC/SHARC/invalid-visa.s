# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2137x %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=sharc -mcpu=adsp-214xx %s 2>&1 | FileCheck %s --check-prefix=TEN

.section .text.visa,"axv"
nop;
# CHECK: :[[@LINE-1]]:1: error: VISA code requires an ADSP-214xx or SHARC+ processor
.isa
.visa
# CHECK: :[[@LINE-1]]:1: error: .visa requires an ADSP-214xx or SHARC+ processor

.section .text.v2,"axv"
IF NE JUMP (PC, -32), ELSE R6 = DM(I5, M0);
# TEN: :[[@LINE-1]]:1: error: Type 10 instructions are not available in VISA code
