# REQUIRES: sharc
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-21371 %s -o %t.21371.o
# RUN: llvm-mc -filetype=obj -triple=sharc -mcpu=adsp-21375 %s -o %t.21375.o
# RUN: ld.lld -T %S/Inputs/sharc-21371-test.ld %t.21371.o --defsym=sym=0x2E4000 -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# RUN: not ld.lld -T %S/Inputs/sharc-21371-test.ld %t.21375.o --defsym=sym=0x2E4000 \
# RUN:   -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

## 0x2E4000 is inside the ADSP-21371's block 1 RAM (0x8000 bytes, data sheet
## Table 3) but past the ADSP-21375's (0x4000 bytes, Table 4).
## 21371: NW 0xB8000 + 0x4000 / 4 = 0xB9000.
# CHECK: R0 = DM(0xb9000);
# ERR: error: {{.*}}R_SHARC_ADDR32_NW against sym: address 0x2E4000 is outside the memory blocks of adsp-21375

.text
.globl _start
_start:
  R0 = DM(sym);
