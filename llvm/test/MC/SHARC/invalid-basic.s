# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2137x %s 2>&1 | FileCheck %s

r0 = r1 +;
# CHECK: :[[@LINE-1]]:1: error: invalid SHARC instruction
r0 = r1 + r2
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error: expected ';'
r0 = q1 + r2;
# CHECK: :[[@LINE-1]]:1: error: invalid SHARC instruction
r0 = f1 + f2;
# CHECK: :[[@LINE-1]]:1: error: invalid SHARC instruction
