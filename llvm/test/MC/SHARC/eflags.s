# RUN: llvm-mc -triple=sharc -mcpu=adsp-2136x -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=M2136X
# RUN: llvm-mc -triple=sharc -mcpu=adsp-2137x -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=M2137X
# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=M214XX
# RUN: llvm-mc -triple=sharc -mcpu=adsp-2156x -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=PLUS
# RUN: llvm-mc -triple=sharc -mcpu=adsp-sc58x -filetype=obj %s -o - | llvm-readobj -h - | FileCheck %s --check-prefix=PLUS

## The disassembler takes the processor from e_flags: SYNC exists only on
## SHARC+.
# RUN: echo "SYNC;" | llvm-mc -triple=sharc -mcpu=adsp-2156x -filetype=obj -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS

nop;

# M2136X: EF_SHARC_MACH_2136X (0x1)
# M2137X: EF_SHARC_MACH_2137X (0x2)
# M214XX: EF_SHARC_MACH_214XX (0x3)
# PLUS:   EF_SHARC_MACH_SHARCPLUS (0x4)
# DIS:    SYNC;
