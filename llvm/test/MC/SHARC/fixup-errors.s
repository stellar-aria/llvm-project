# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2137x -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

JUMP (PC, odd);
# CHECK: :[[@LINE-1]]:11: error: branch target is not on an instruction boundary
.byte 0
odd: NOP;
