# RUN: llvm-mc -triple=sharc -mcpu=adsp-2136x -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=M2136X
# RUN: llvm-mc -triple=sharc -mcpu=adsp-2137x -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=M2137X
# RUN: llvm-mc -triple=sharc -mcpu=adsp-21371 -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=P21371
# RUN: llvm-mc -triple=sharc -mcpu=adsp-21375 -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=P21375
# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=M214XX
# RUN: llvm-mc -triple=sharc -mcpu=adsp-2156x -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=PLUS
# RUN: llvm-mc -triple=sharc -mcpu=adsp-sc58x -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=PLUS

## The disassembler takes the processor from e_flags: SYNC exists only on
## SHARC+.
# RUN: echo "SYNC;" | llvm-mc -triple=sharc -mcpu=adsp-2156x -filetype=obj -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS

## The part is a processor name objdump can take from e_flags.
# RUN: echo "nop;" | llvm-mc -triple=sharc -mcpu=adsp-21375 -filetype=obj -o %t.21375.o
# RUN: llvm-objdump -d %t.21375.o | FileCheck %s --check-prefix=DIS21375

nop;

# M2136X: EF_SHARC_MACH_2136X (0x1)
# M2137X: EF_SHARC_MACH_2137X (0x2)
# P21371: Flags [ (0x12)
# P21371-NEXT: EF_SHARC_MACH_2137X (0x2)
# P21371-NEXT: EF_SHARC_PART_21371 (0x10)
# P21375: Flags [ (0x22)
# P21375-NEXT: EF_SHARC_MACH_2137X (0x2)
# P21375-NEXT: EF_SHARC_PART_21375 (0x20)
# M214XX: EF_SHARC_MACH_214XX (0x3)
# PLUS:   EF_SHARC_MACH_SHARCPLUS (0x4)
# DIS:    SYNC;
# DIS21375: NOP;
