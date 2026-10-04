# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d %t.o | FileCheck %s
#
# A PC-relative jump to a label starts as the 32-bit Type 9b with a 6-bit
# offset and becomes the 48-bit Type 8a when the target is out of range.

.text
.visa
JUMP (PC, near);
nop;
near: nop;
JUMP (PC, far);
.rept 40
nop;
.endr
far: nop;

# CHECK:      0: {{[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ [0-9a-f]+}} JUMP (PC, 3);
# CHECK:      8: {{([0-9a-f]+ ){5}[0-9a-f]+}} JUMP (PC, 43);
