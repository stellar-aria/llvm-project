# REQUIRES: sharc
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-2156x %s -o %t.plus.o
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-21371 %s -o %t.21371.o
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-21375 %s -o %t.21375.o
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-2137x %s -o %t.generic.o

## EF_SHARC_MACH_SHARCPLUS is 4 (llvm/include/llvm/BinaryFormat/ELF.h).
# RUN: ld.lld -T %S/Inputs/sharc-2156x-test.ld %t.plus.o -o %t
# RUN: llvm-readelf -h %t | FileCheck %s --check-prefix=PLUS
# PLUS: Flags: 0x4

## An object without a part links with one that has a part, and the output
## carries it: EF_SHARC_MACH_2137X (2) | EF_SHARC_PART_21371 (0x10).
# RUN: ld.lld -T %S/Inputs/sharc-21371-test.ld %t.21371.o %t.generic.o -o %t.classic
# RUN: llvm-readelf -h %t.classic | FileCheck %s --check-prefix=PART
# PART: Flags: 0x12

# RUN: not ld.lld %t.plus.o %t.21371.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=MACH
# MACH: error: {{.*}}21371.o: cannot link object files for different SHARC processors

# RUN: not ld.lld %t.21371.o %t.21375.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=PARTS
# PARTS: error: {{.*}}21375.o: cannot link object files for different SHARC parts

# RUN: not ld.lld %t.generic.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=NOPART
# NOPART: error: no SHARC memory map for these objects; assemble classic code with -mcpu=adsp-21371 or -mcpu=adsp-21375

.text
  NOP;
