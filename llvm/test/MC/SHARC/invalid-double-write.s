# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2137x %s -o /dev/null 2>&1 | FileCheck %s
#
# A dual move may not load one register from both buses (PRM 7-28 note).

R0 = DM(I0, M0), R0 = PM(I8, M8);
# CHECK: :[[@LINE-1]]:1: error: both memory moves load R0
R0 = DM(I0, M0), R1 = PM(I8, M8);
# CHECK-NOT: :[[@LINE-1]]:{{.*}}error
