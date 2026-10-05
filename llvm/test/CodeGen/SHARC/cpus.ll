; Check the processor and feature tables, and that the target machine can be
; created (MCAsmInfo and Triple must agree on exception handling).
; RUN: llc < %s -mtriple=sharc -mcpu=help -o /dev/null 2>&1 | FileCheck %s

; CHECK: Available CPUs for this target:
; CHECK: adsp-2136x
; CHECK: adsp-21371
; CHECK: adsp-21375
; CHECK: adsp-2137x
; CHECK: adsp-214xx
; CHECK: adsp-2156x
; CHECK: adsp-2158x
; CHECK: adsp-sc58x
; CHECK: Available features for this target:
; CHECK: byte-addr
; CHECK: fp64
; CHECK: sharcplus
; CHECK: visa
