# RUN: llvm-mc -triple=sharc -mcpu=adsp-2137x -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s
# RUN: llvm-mc -triple=sharc -mcpu=adsp-2137x -show-inst %s | FileCheck %s --check-prefix=INST
#
# Symbols may share a name with a syntax keyword (F, CI, LCE, ...), but a
# reading without such a symbol is preferred: R3 = MR0F is a multiplier
# register move, not R3 = <address of MR0F>.

JUMP f;
JUMP ci;
R0 = lce;
R3 = MR0F;

# CHECK:      0x0 R_SHARC_ADDR24_NW f 0x0
# CHECK-NEXT: 0x6 R_SHARC_ADDR24_NW ci 0x0
# CHECK-NEXT: 0xC R_SHARC_ADDR32_NW lce 0x0
# CHECK-NEXT: }
# INST: R3 = MR0F; {{.*}}<MCInst #{{[0-9]+}} T2a
