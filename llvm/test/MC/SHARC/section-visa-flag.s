# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -filetype=obj %s -o %t.o
# RUN: llvm-readobj -S %t.o | FileCheck %s
# RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS
# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx %s | FileCheck %s --check-prefix=ASM
#
# A section with SHF_SHARC_VISA ("v") holds VISA code; others hold ISA code.
# The same instruction encodes as 48 bits in one and 16 bits in the other.

.section .text.isa,"ax"
r0 = r0 + r1;
.section .text.visa,"axv"
r0 = r0 + r1;
.section .text.later,"ax"
.visa
r0 = r0 + r1;

# CHECK:      Name: .text.isa
# CHECK:      Flags [ (0x6)
# CHECK:      Name: .text.visa
# CHECK:      Flags [ (0x10000006)
# CHECK:        SHF_SHARC_VISA (0x10000000)
# CHECK:      Name: .text.later
# CHECK:      Flags [ (0x10000006)

# DIS:      <.text.isa>:
# DIS-NEXT: 3e 01 00 00 01 10 {{.*}}R0 = R0 + R1;
# DIS:      <.text.visa>:
# DIS-NEXT: 01 c0 {{.*}}R0 = R0 + R1;
# DIS:      <.text.later>:
# DIS-NEXT: 01 c0 {{.*}}R0 = R0 + R1;

# ASM: .section .text.visa,"axv",@progbits
