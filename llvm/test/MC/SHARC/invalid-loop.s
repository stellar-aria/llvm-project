# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2156x -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

LCNTR = 2, DO nowhere UNTIL LCE;
# CHECK: :[[@LINE-1]]:15: error: hardware loop end 'nowhere' is not defined in this section
