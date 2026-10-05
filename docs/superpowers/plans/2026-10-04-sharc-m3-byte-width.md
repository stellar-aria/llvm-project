# SHARC M3: Non-8-bit Bytes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** LLVM IR and the optimizer handle `b:32` modules correctly for what classic-SHARC CodeGen (M4) needs.

**Architecture:** Import Sergei Barannikov's 8-commit upstream stack (DataLayout `b:`, memset, isBytewiseValue, getByteTy/ptradd, m_PtrAdd, string info, SimplifyLibCalls), then drive a `b:32` IR corpus through the optimizer, fixing or guarding each pass that assumes 8-bit bytes, one generic commit per pass.

**Tech Stack:** C++ (LLVM IR, Analysis, Transforms), lit/FileCheck, gtest.

**Spec:** `docs/superpowers/specs/2026-10-04-sharc-m3-byte-width-design.md`

## Global Constraints

- Upstream commits are imported with `git cherry-pick -x`, author kept (Sergei Barannikov); conflict resolutions are described in the commit message under "Conflicts:".
- Generic commits (stack, pass fixes, corpus) are kept apart from SHARC target commits; none touch `llvm/lib/Target/SHARC`.
- Expected values in tests are computed by hand (shown in comments), never captured from tool output.
- 8-bit behaviour is unchanged: each touched pass's existing tests stay green.
- The `sharc` branch is downstream; never merge to or PR against LLVM `main`.
- Build dir `build-sharc` (Ninja, assertions on, ccache). Lit: `build-sharc/bin/llvm-lit`.
- Corpus DataLayout (spec §3): `e-m:e-b:32-p:32:32-i64:32-f32:32-f64:32-a:0:32-n32`. With `b:32` the defaults are cleared (only `i32` is preset), so every type used needs a spec; the corpus uses only `i1`, `i32`, `i64`, `float`, `double`, pointers and aggregates of them.
- Under `b:32`: `i32` is 1 byte, `i64` and `double` are 2 bytes, an `i8` or `i16` still occupies 1 whole byte; `llvm.memset`'s value operand is `i32`; `IRBuilder::CreatePtrAdd` emits `getelementptr i32` (the byte type).

## Facts used by the tests

Under the corpus layout (hand-derived from the DataLayout patch: sizes are bits / 32, rounded up):

| Type | Store size (bytes) | Alloc size (bytes) |
|---|---|---|
| `i32`, `float`, `ptr`, `i8`, `i16`, `i1` | 1 | 1 |
| `i64`, `double` | 2 | 2 |
| `{ i32, i32, i32, i32 }` | 4 | 4 |
| `[6 x i32]` | 6 | 6 |
| `{ i32, i64 }` | 3 | 3 (i64 aligned to 32 bits = 1 byte) |

---

### Task 1: Import the upstream stack

**Files:**
- Modify: whatever each upstream commit touches (see spec §2 table).
- Create: `llvm/unittests/IR/DataLayoutByteWidthTest.cpp` (the DataLayout patch has no tests of its own).
- Modify: `llvm/unittests/IR/CMakeLists.txt`.

**Interfaces:**
- Produces (from the stack): `unsigned DataLayout::getByteWidth() const`; `IntegerType *IRBuilderBase::getByteTy()`; `CharWidth` argument of `getConstantStringInfo`; `llvm.memset` value operand typed `iN` where N is the byte width.

- [ ] **Step 1: Make the stack commits available.**
```bash
cd /home/kate/GitHub/llvm-project
for i in 1-datalayout 2-memset-intrinsic 3-bytewise-value 4-create-ptradd 5-match-ptradd 6-string-info-char-width 7-simplify-lib-calls; do
  git fetch -q https://github.com/llvm/llvm-project.git "refs/heads/users/s-barannikov/byte/${i}:refs/remotes/byte/${i}"
done
git log --reverse --format='%h %s' byte/1-datalayout~1..byte/7-simplify-lib-calls
```
Expected: the 8 commits of spec §2 (63fa6c81d677 … 21602592d581). (`${i}` braces matter: zsh reads `$i:r` as a modifier.)

- [ ] **Step 2: Import commit 1 (DataLayout) and add its unit test.**
```bash
git cherry-pick -x 63fa6c81d677
```
Then write `llvm/unittests/IR/DataLayoutByteWidthTest.cpp`:

```cpp
//===- DataLayoutByteWidthTest.cpp - DataLayout byte width ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

// The layout of the SHARC byte-width corpus (llvm/test/Transforms/ByteWidth).
const char *Layout32 = "e-m:e-b:32-p:32:32-i64:32-f32:32-f64:32-a:0:32-n32";

TEST(DataLayoutByteWidth, Parse) {
  EXPECT_EQ(DataLayout().getByteWidth(), 8u);
  auto DL = DataLayout::parse(Layout32);
  ASSERT_THAT_EXPECTED(DL, Succeeded());
  EXPECT_EQ(DL->getByteWidth(), 32u);
  EXPECT_THAT_EXPECTED(DataLayout::parse("b:12"), Failed());
  EXPECT_THAT_EXPECTED(DataLayout::parse("b:"), Failed());
}

TEST(DataLayoutByteWidth, Sizes) {
  LLVMContext C;
  DataLayout DL = cantFail(DataLayout::parse(Layout32));
  // Sizes are bits / 32, rounded up to whole bytes.
  EXPECT_EQ(DL.getTypeStoreSize(Type::getInt32Ty(C)).getFixedValue(), 1u);
  EXPECT_EQ(DL.getTypeStoreSize(Type::getInt64Ty(C)).getFixedValue(), 2u);
  EXPECT_EQ(DL.getTypeStoreSize(Type::getInt8Ty(C)).getFixedValue(), 1u);
  EXPECT_EQ(DL.getTypeAllocSize(PointerType::get(C, 0)).getFixedValue(), 1u);
  EXPECT_EQ(DL.getTypeAllocSizeInBits(Type::getInt64Ty(C)).getFixedValue(),
            64u);
  auto *S = StructType::get(C, {Type::getInt32Ty(C), Type::getInt64Ty(C)});
  EXPECT_EQ(DL.getTypeAllocSize(S).getFixedValue(), 3u);
  EXPECT_EQ(DL.getStructLayout(S)->getElementOffset(1).getFixedValue(), 1u);
  auto *A = ArrayType::get(Type::getInt32Ty(C), 6);
  EXPECT_EQ(DL.getTypeAllocSize(A).getFixedValue(), 6u);
}

TEST(DataLayoutByteWidth, RoundTrip) {
  DataLayout DL = cantFail(DataLayout::parse(Layout32));
  DataLayout Again = cantFail(DataLayout::parse(DL.getStringRepresentation()));
  EXPECT_EQ(Again.getByteWidth(), 32u);
  EXPECT_EQ(DL, Again);
}

} // namespace
```

Add it to `llvm/unittests/IR/CMakeLists.txt`. Run:
`ninja -C build-sharc IRTests && build-sharc/unittests/IR/IRTests --gtest_filter='DataLayoutByteWidth*'`
Expected: 3 PASS. If `getStringRepresentation` drops `b:32`, that is a bug in the patch: fix it in a separate commit ("[DataLayout] Keep the byte spec in the string representation") with this test. Then run `build-sharc/bin/llvm-lit -q llvm/test/Assembler llvm/test/Verifier` (no failures) and commit the test:
```bash
git add llvm/unittests/IR/DataLayoutByteWidthTest.cpp llvm/unittests/IR/CMakeLists.txt
git commit -m "[DataLayout] Test the byte specification"
```

- [ ] **Step 3: Import commits 2-8 one at a time.** For each commit `C` in 389835147eab, 611ae7ec7052, 389c7661aa94, 966835daf0e3, 2e73d7ff5209, 3ccd33f9471e, 21602592d581:
  1. `git cherry-pick -x C`.
  2. On conflict, resolve against today's code keeping the commit's intent (read `git show C` for the intent; for moved code, apply the same change at the new location). Known conflict files from the probe: commit 2 — `Intrinsics.h`, `Intrinsics.td`, `Intrinsics.cpp`, `Verifier.cpp`; 4 — `SanitizerCoverage.cpp`, `IRBuilderTest.cpp`; 5 — `Constants.h`, `ScalarEvolution.cpp`, `Constants.cpp`; 6 — `PatternMatch.cpp`; 7 — Clang `TargetBuiltins/AMDGPU.cpp`; 8 — `InstCombineCalls.cpp`.
  3. `git add` the resolved files, then `git -c core.editor=true cherry-pick --continue`, then amend the message to add a paragraph `Conflicts: <file>: <what moved and how it was resolved>` (`git commit --amend`, keeping the `(cherry picked from commit …)` line and the author).
  4. Build: `ninja -C build-sharc opt llc llvm-as llvm-dis IRTests AnalysisTests TransformUtilsTests`.
  5. Test: the commit's own test files (`git show --stat C`) with `llvm-lit -v`, plus `build-sharc/bin/llvm-lit -q llvm/test/Assembler llvm/test/Verifier llvm/test/Analysis/ValueTracking llvm/test/Transforms/InstCombine llvm/test/Transforms/InstSimplify` and the relevant unit tests. Expected: no failures. A failure caused by today's code (a newly added call of a changed API, e.g. a new `getConstantStringInfo` caller) is fixed inside the same commit and listed under "Conflicts:".
  6. Commit 7 touches Clang, which `build-sharc` does not build. Check it with `git show C -- clang` that the change is the mechanical argument addition; note "Clang part not built" in the message.

- [ ] **Step 4: Check the stack as a whole.**
Run: `ninja -C build-sharc && build-sharc/bin/llvm-lit -q llvm/test/Transforms llvm/test/Analysis llvm/test/IR llvm/test/Assembler llvm/test/Verifier`
Expected: no failures. Record the counts for Task 7.

---

### Task 2: Strings and library calls

**Files:**
- Create: `llvm/test/Transforms/ByteWidth/strings.ll`, `llvm/test/Transforms/ByteWidth/lit.local.cfg` (none needed if the directory inherits; add only if lit requires it)

**Interfaces:**
- Consumes: Task 1's stack (SimplifyLibCalls char width, getConstantStringInfo).

- [ ] **Step 1: Write the test.**

```llvm
; Strings on a target with 32-bit bytes: a char is one 32-bit byte, strings
; are [N x i32], and size_t is i32.
; RUN: opt -passes=instcombine -S %s | FileCheck %s
; RUN: opt -O0 -verify-each -disable-output %s
; RUN: opt -O1 -verify-each -disable-output %s
; RUN: opt -O2 -verify-each -disable-output %s
; RUN: opt -O3 -verify-each -disable-output %s

target datalayout = "e-m:e-b:32-p:32:32-i64:32-f32:32-f64:32-a:0:32-n32"

@hello = private constant [6 x i32] [i32 104, i32 101, i32 108, i32 108, i32 111, i32 0]
@abc = private constant [4 x i32] [i32 97, i32 98, i32 99, i32 0]
@abd = private constant [4 x i32] [i32 97, i32 98, i32 100, i32 0]
; A character that does not fit in 8 bits: 0x263A.
@wide = private constant [3 x i32] [i32 9786, i32 65, i32 0]

declare i32 @strlen(ptr)
declare i32 @strcmp(ptr, ptr)
declare ptr @strchr(ptr, i32)
declare void @llvm.memcpy.p0.p0.i32(ptr, ptr, i32, i1)

; "hello" has 5 characters.
; CHECK-LABEL: @len(
; CHECK: ret i32 5
define i32 @len() {
  %n = call i32 @strlen(ptr @hello)
  ret i32 %n
}

; A 32-bit character is one character, not four.
; CHECK-LABEL: @len_wide(
; CHECK: ret i32 2
define i32 @len_wide() {
  %n = call i32 @strlen(ptr @wide)
  ret i32 %n
}

; "abc" < "abd".
; CHECK-LABEL: @cmp(
; CHECK: ret i32 -1
define i32 @cmp() {
  %r = call i32 @strcmp(ptr @abc, ptr @abd)
  ret i32 %r
}

; strchr("hello", 'l') is @hello + 2 bytes (two 32-bit characters).
; CHECK-LABEL: @chr(
; CHECK: ret ptr getelementptr inbounds nuw (i32, ptr @hello, i32 2)
define ptr @chr() {
  %p = call ptr @strchr(ptr @hello, i32 108)
  ret ptr %p
}

; Copying the string is 6 bytes; whatever instcombine does with it, no
; 8-bit types appear.
; CHECK-LABEL: @copy(
; CHECK-NOT: i8
; CHECK: ret void
define void @copy(ptr %d) {
  call void @llvm.memcpy.p0.p0.i32(ptr %d, ptr @hello, i32 6, i1 false)
  ret void
}
```

The `@chr` CHECK line fixes the canonical form loosely; if the folded constant uses a different but equivalent GEP spelling (e.g. without `nuw`, or `[6 x i32], ptr @hello, i32 0, i32 2`), accept it only if it still means @hello + 2 bytes, and write that spelling in the CHECK with a comment.

- [ ] **Step 2: Run it.**
Run: `build-sharc/bin/llvm-lit -v llvm/test/Transforms/ByteWidth/strings.ll`
Expected: PASS, since SimplifyLibCalls support is in the stack. For any failing fact, follow the triage procedure (below) for the pass responsible.

- [ ] **Step 3: Commit** (test plus any fix commits made by the triage procedure):
```bash
git add llvm/test/Transforms/ByteWidth/strings.ll
git commit -m "[ByteWidth] Test string library calls with 32-bit bytes"
```

#### Triage procedure (used by Tasks 2-6)

When a corpus fact fails or an `-O` run crashes:

1. Find the pass: rerun with `-print-after-all` (fact) or read the crash stack (crash); reduce with `llvm-reduce --test` if the case is large.
2. Find the 8-bit assumption: look in the pass for `* 8`, `/ 8`, `<< 3`, `getInt8Ty`, `getInt8PtrTy`, `Type::getIntNTy(C, Size * 8)`, `getTypeStoreSize(...) * 8`, `alignTo(..., 8)`.
3. Decide:
   - **Fix** when the change is local (replace 8 with `DL.getByteWidth()`, `getInt8Ty()` with `Builder.getByteTy()` / `Type::getIntNTy(C, DL.getByteWidth())`).
   - **Guard** when the pass relies on 8-bit bytes throughout:
     ```cpp
     // This transformation assumes 8-bit bytes.
     if (DL.getByteWidth() != 8)
       return false;
     ```
     at the transformation's entry (or the narrowest function that needs it), and add the item to spec §6 "Deferred items".
4. Add a focused test in the pass's own directory, e.g. `llvm/test/Transforms/SROA/byte-width-32.ll`, with the `b:32` layout and a hand-computed CHECK; for a guard, CHECK that the input is unchanged.
5. Run the pass's existing test directory (`llvm-lit -q llvm/test/Transforms/<Pass>`): no new failures.
6. Commit separately: `git commit -m "[<Pass>] Support non-8-bit bytes in <what>"` or `"[<Pass>] Skip <what> for non-8-bit bytes"`.
7. Record the outcome (fixed / guarded / unaffected) for the audit table (Task 7).

---

### Task 3: Aggregates, SROA and memcpy/memset

**Files:**
- Create: `llvm/test/Transforms/ByteWidth/aggregates.ll`, `llvm/test/Transforms/ByteWidth/sroa.ll`

- [ ] **Step 1: Write** `aggregates.ll`:

```llvm
; Struct copy and zero-initialization with 32-bit bytes: %S is 4 bytes.
; RUN: opt -passes=instcombine -S %s | FileCheck %s --check-prefix=IC
; RUN: opt -passes=memcpyopt -S %s | FileCheck %s --check-prefix=MCO
; RUN: opt -O0 -verify-each -disable-output %s
; RUN: opt -O1 -verify-each -disable-output %s
; RUN: opt -O2 -verify-each -disable-output %s
; RUN: opt -O3 -verify-each -disable-output %s

target datalayout = "e-m:e-b:32-p:32:32-i64:32-f32:32-f64:32-a:0:32-n32"

%S = type { i32, i32, i32, i32 }

declare void @llvm.memcpy.p0.p0.i32(ptr, ptr, i32, i1)
declare void @llvm.memset.p0.i32(ptr, i32, i32, i1)

; A one-byte memcpy is one i32 load and store (not i8).
; IC-LABEL: @copy1(
; IC: %[[V:.*]] = load i32, ptr %s
; IC: store i32 %[[V]], ptr %d
define void @copy1(ptr %d, ptr %s) {
  call void @llvm.memcpy.p0.p0.i32(ptr %d, ptr %s, i32 1, i1 false)
  ret void
}

; A two-byte memcpy is 64 bits.
; IC-LABEL: @copy2(
; IC: load i64, ptr %s
; IC: store i64
define void @copy2(ptr %d, ptr %s) {
  call void @llvm.memcpy.p0.p0.i32(ptr %d, ptr %s, i32 2, i1 false)
  ret void
}

; A one-byte memset of 7 is a store of i32 7.
; IC-LABEL: @set1(
; IC: store i32 7, ptr %d
define void @set1(ptr %d) {
  call void @llvm.memset.p0.i32(ptr %d, i32 7, i32 1, i1 false)
  ret void
}

; GEP over %S by 1 element is 4 bytes.
; IC-LABEL: @next(
; IC: getelementptr {{.*}}i32, ptr %p, i32 4
define ptr @next(ptr %p) {
  %q = getelementptr %S, ptr %p, i32 1
  ret ptr %q
}

; Four zero stores to consecutive bytes become one 4-byte memset of i32 0.
; MCO-LABEL: @zero(
; MCO: call void @llvm.memset.p0.i32(ptr {{.*}}%p, i32 0, i32 4, i1 false)
define void @zero(ptr %p) {
  store i32 0, ptr %p
  %p1 = getelementptr i32, ptr %p, i32 1
  store i32 0, ptr %p1
  %p2 = getelementptr i32, ptr %p, i32 2
  store i32 0, ptr %p2
  %p3 = getelementptr i32, ptr %p, i32 3
  store i32 0, ptr %p3
  ret void
}

; A struct copy through a temporary: memcpy-memcpy forwarding keeps 4 bytes.
; MCO-LABEL: @forward(
; MCO: call void @llvm.memcpy.p0.p0.i32(ptr {{.*}}%d, ptr {{.*}}%s, i32 4, i1 false)
define void @forward(ptr %d, ptr %s) {
  %t = alloca %S
  call void @llvm.memcpy.p0.p0.i32(ptr %t, ptr %s, i32 4, i1 false)
  call void @llvm.memcpy.p0.p0.i32(ptr %d, ptr %t, i32 4, i1 false)
  ret void
}
```

MemCpyOpt's store merging needs at least a threshold of stores (check `MemCpyOpt`'s merge size limit); if four i32 stores are below it, add stores (8 of them, memset length 8) and update the CHECK and the comment.

- [ ] **Step 2: Write** `sroa.ll`:

```llvm
; SROA splits a struct alloca of 32-bit-byte fields.
; RUN: opt -passes=sroa -S %s | FileCheck %s
; RUN: opt -O2 -verify-each -disable-output %s

target datalayout = "e-m:e-b:32-p:32:32-i64:32-f32:32-f64:32-a:0:32-n32"

; { i32, i64 }: the i64 is at byte 1 and is 2 bytes. Storing a then b and
; reading them back promotes both to SSA values.
; CHECK-LABEL: @pair(
; CHECK-NOT: alloca
; CHECK: %[[S:.*]] = add i64 %b, {{.*}}
; CHECK: ret i64 %[[S]]
define i64 @pair(i32 %a, i64 %b) {
  %t = alloca { i32, i64 }
  store i32 %a, ptr %t
  %f1 = getelementptr { i32, i64 }, ptr %t, i32 0, i32 1
  store i64 %b, ptr %f1
  %x = load i32, ptr %t
  %y = load i64, ptr %f1
  %x64 = zext i32 %x to i64
  %s = add i64 %y, %x64
  ret i64 %s
}

; Reading the low half of an i64 field as i32 (little-endian: byte 1 of the
; struct is bits 0-31 of %b) is a truncation.
; CHECK-LABEL: @low(
; CHECK-NOT: alloca
; CHECK: trunc i64 %b to i32
define i32 @low(i64 %b) {
  %t = alloca { i32, i64 }
  %f1 = getelementptr { i32, i64 }, ptr %t, i32 0, i32 1
  store i64 %b, ptr %f1
  %lo = load i32, ptr %f1
  ret i32 %lo
}

; The high half is at byte 2: a shift by 32 bits, not by 8.
; CHECK-LABEL: @high(
; CHECK-NOT: alloca
; CHECK: lshr i64 %b, 32
define i32 @high(i64 %b) {
  %t = alloca { i32, i64 }
  %f1 = getelementptr { i32, i64 }, ptr %t, i32 0, i32 1
  store i64 %b, ptr %f1
  %hi.p = getelementptr i32, ptr %f1, i32 1
  %hi = load i32, ptr %hi.p
  ret i32 %hi
}
```

- [ ] **Step 3: Run both.**
Run: `build-sharc/bin/llvm-lit -v llvm/test/Transforms/ByteWidth/aggregates.ll llvm/test/Transforms/ByteWidth/sroa.ll`
Expected: failures are likely (InstCombine's memcpy-to-load/store and SROA's integer widening use 8-bit arithmetic). Apply the triage procedure to each until both pass.

- [ ] **Step 4: Commit** the corpus files (fix commits are made by triage):
```bash
git add llvm/test/Transforms/ByteWidth/aggregates.ll llvm/test/Transforms/ByteWidth/sroa.ll
git commit -m "[ByteWidth] Test aggregates and SROA with 32-bit bytes"
```

---

### Task 4: Pointers and globals

**Files:**
- Create: `llvm/test/Transforms/ByteWidth/pointers.ll`, `llvm/test/Transforms/ByteWidth/globals.ll`

- [ ] **Step 1: Write** `pointers.ll`:

```llvm
; GEP canonicalization counts offsets in 32-bit bytes.
; RUN: opt -passes=instcombine -S %s | FileCheck %s
; RUN: opt -O2 -verify-each -disable-output %s

target datalayout = "e-m:e-b:32-p:32:32-i64:32-f32:32-f64:32-a:0:32-n32"

; Element 7 of [10 x i32] is 7 bytes in.
; CHECK-LABEL: @arr(
; CHECK: getelementptr {{.*}}i32, ptr %a, i32 7
define ptr @arr(ptr %a) {
  %p = getelementptr [10 x i32], ptr %a, i32 0, i32 7
  ret ptr %p
}

; One i64 is 2 bytes.
; CHECK-LABEL: @i64step(
; CHECK: getelementptr {{.*}}i32, ptr %p, i32 2
define ptr @i64step(ptr %p) {
  %q = getelementptr i64, ptr %p, i32 1
  ret ptr %q
}

; An i8 occupies a whole byte: one i8 is 1 byte.
; CHECK-LABEL: @i8step(
; CHECK: getelementptr {{.*}}i32, ptr %p, i32 1
define ptr @i8step(ptr %p) {
  %q = getelementptr i8, ptr %p, i32 1
  ret ptr %q
}

; Two GEPs combine: 3 bytes + 2 * 2 bytes = 7.
; CHECK-LABEL: @combine(
; CHECK: getelementptr {{.*}}i32, ptr %p, i32 7
define ptr @combine(ptr %p) {
  %a = getelementptr i32, ptr %p, i32 3
  %b = getelementptr i64, ptr %a, i32 2
  ret ptr %b
}

; Pointer difference of two GEPs is in bytes: (5 - 2) = 3.
; CHECK-LABEL: @diff(
; CHECK: ret i32 3
define i32 @diff(ptr %p) {
  %a = getelementptr i32, ptr %p, i32 5
  %b = getelementptr i32, ptr %p, i32 2
  %ai = ptrtoint ptr %a to i32
  %bi = ptrtoint ptr %b to i32
  %d = sub i32 %ai, %bi
  ret i32 %d
}
```

- [ ] **Step 2: Write** `globals.ll`:

```llvm
; Loads from constant globals and store-to-load forwarding with 32-bit bytes.
; RUN: opt -passes=instcombine -S %s | FileCheck %s --check-prefix=IC
; RUN: opt -passes=gvn -S %s | FileCheck %s --check-prefix=GVN
; RUN: opt -O2 -verify-each -disable-output %s

target datalayout = "e-m:e-b:32-p:32:32-i64:32-f32:32-f64:32-a:0:32-n32"

@g = constant [4 x i32] [i32 1, i32 2, i32 3, i32 4]
@pair = constant { i32, i64 } { i32 9, i64 81985529216486895 } ; 0x0123456789ABCDEF

; Element 2 is 3.
; IC-LABEL: @elt(
; IC: ret i32 3
define i32 @elt() {
  %p = getelementptr i32, ptr @g, i32 2
  %v = load i32, ptr %p
  ret i32 %v
}

; An i64 load at byte 1 reads elements 1 and 2, little-endian:
; 2 | 3 << 32 = 12884901890.
; IC-LABEL: @wide(
; IC: ret i64 12884901890
define i64 @wide() {
  %p = getelementptr i32, ptr @g, i32 1
  %v = load i64, ptr %p
  ret i64 %v
}

; The high 32 bits of the i64 field (byte 2 of @pair) are 0x01234567.
; IC-LABEL: @field_hi(
; IC: ret i32 19088743
define i32 @field_hi() {
  %p = getelementptr i32, ptr @pair, i32 2
  %v = load i32, ptr %p
  ret i32 %v
}

; Forwarding a stored i64 to a load of its second byte: bits 32-63.
; GVN-LABEL: @fwd(
; GVN: %[[H:.*]] = lshr i64 %x, 32
; GVN: %[[T:.*]] = trunc {{.*}}i64 %[[H]] to i32
; GVN: ret i32 %[[T]]
define i32 @fwd(ptr %p, i64 %x) {
  store i64 %x, ptr %p
  %q = getelementptr i32, ptr %p, i32 1
  %v = load i32, ptr %q
  ret i32 %v
}
```

- [ ] **Step 3: Run both.**
Run: `build-sharc/bin/llvm-lit -v llvm/test/Transforms/ByteWidth/pointers.ll llvm/test/Transforms/ByteWidth/globals.ll`
Expected: failures point at ConstantFolding (`ConstantFoldLoadFromConst` reads bytes) and GVN's VNCoercion. Triage each.

- [ ] **Step 4: Commit.**
```bash
git add llvm/test/Transforms/ByteWidth/pointers.ll llvm/test/Transforms/ByteWidth/globals.ll
git commit -m "[ByteWidth] Test pointer arithmetic and constant loads with 32-bit bytes"
```

---

### Task 5: Loops

**Files:**
- Create: `llvm/test/Transforms/ByteWidth/loop-idioms.ll`

- [ ] **Step 1: Write the test.**

```llvm
; Loop idioms with 32-bit bytes: an i32 element is one byte, so lengths are
; element counts and any i32 value is "bytewise".
; RUN: opt -passes=loop-idiom -S %s | FileCheck %s
; RUN: opt -passes='loop-reduce' -S %s | FileCheck %s --check-prefix=LSR
; RUN: opt -O2 -verify-each -disable-output %s
; RUN: opt -O3 -verify-each -disable-output %s

target datalayout = "e-m:e-b:32-p:32:32-i64:32-f32:32-f64:32-a:0:32-n32"

; for (i = 0; i < n; i++) a[i] = 0;  ->  memset(a, 0, n)
; CHECK-LABEL: @zero(
; CHECK: call void @llvm.memset.p0.i32(ptr {{.*}}%a, i32 0, i32 %n, i1 false)
define void @zero(ptr %a, i32 %n) {
entry:
  %c = icmp sgt i32 %n, 0
  br i1 %c, label %loop, label %exit
loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]
  %p = getelementptr i32, ptr %a, i32 %i
  store i32 0, ptr %p
  %i.next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %i.next, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; Any i32 is a single byte, so storing 7 is also a memset (of i32 7).
; CHECK-LABEL: @seven(
; CHECK: call void @llvm.memset.p0.i32(ptr {{.*}}%a, i32 7, i32 %n, i1 false)
define void @seven(ptr %a, i32 %n) {
entry:
  %c = icmp sgt i32 %n, 0
  br i1 %c, label %loop, label %exit
loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]
  %p = getelementptr i32, ptr %a, i32 %i
  store i32 7, ptr %p
  %i.next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %i.next, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; for (i = 0; i < n; i++) d[i] = s[i];  ->  memcpy(d, s, n)
; CHECK-LABEL: @copy(
; CHECK: call void @llvm.memcpy.p0.p0.i32(ptr {{.*}}%d, ptr {{.*}}%s, i32 %n, i1 false)
define void @copy(ptr noalias %d, ptr noalias %s, i32 %n) {
entry:
  %c = icmp sgt i32 %n, 0
  br i1 %c, label %loop, label %exit
loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]
  %ps = getelementptr i32, ptr %s, i32 %i
  %v = load i32, ptr %ps
  %pd = getelementptr i32, ptr %d, i32 %i
  store i32 %v, ptr %pd
  %i.next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %i.next, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; An i64 array: element i is 2 * i bytes; memset length is 2 * n.
; CHECK-LABEL: @zero64(
; CHECK: %[[L:.*]] = shl nuw {{.*}}i32 %n, 1
; CHECK: call void @llvm.memset.p0.i32(ptr {{.*}}%a, i32 0, i32 %[[L]], i1 false)
define void @zero64(ptr %a, i32 %n) {
entry:
  %c = icmp sgt i32 %n, 0
  br i1 %c, label %loop, label %exit
loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]
  %p = getelementptr i64, ptr %a, i32 %i
  store i64 0, ptr %p
  %i.next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %i.next, %n
  br i1 %done, label %exit, label %loop
exit:
  ret void
}

; LSR steps the pointer by 2 bytes per i64 element.
; LSR-LABEL: @sum64(
; LSR: getelementptr {{.*}}i32, ptr %{{.*}}, i32 2
define i64 @sum64(ptr %a, i32 %n) {
entry:
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]
  %s = phi i64 [ 0, %entry ], [ %s.next, %loop ]
  %p = getelementptr i64, ptr %a, i32 %i
  %v = load i64, ptr %p
  %s.next = add i64 %s, %v
  %i.next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %i.next, %n
  br i1 %done, label %exit, label %loop
exit:
  ret i64 %s.next
}
```

`shl nuw i32 %n, 1` may be spelled `mul` or have other flags; match the meaning (2 * n) and adjust the CHECK, with a comment. LSR's output depends on TTI; without a target triple it may keep the indexed form. If LSR does not produce a pointer increment, replace the LSR fact with a robustness-only RUN and record "LSR: not exercised without a target" for M4.

- [ ] **Step 2: Run** `build-sharc/bin/llvm-lit -v llvm/test/Transforms/ByteWidth/loop-idioms.ll` and triage failures (LoopIdiomRecognize computes store sizes and splat values).

- [ ] **Step 3: Commit.**
```bash
git add llvm/test/Transforms/ByteWidth/loop-idioms.ll
git commit -m "[ByteWidth] Test loop idioms with 32-bit bytes"
```

---

### Task 6: Store merging and switch tables

**Files:**
- Create: `llvm/test/Transforms/ByteWidth/store-merge.ll`, `llvm/test/Transforms/ByteWidth/switch-table.ll`

- [ ] **Step 1: Write** `store-merge.ll`:

```llvm
; Merging adjacent stores and loads with 32-bit bytes.
; RUN: opt -passes=instcombine -S %s | FileCheck %s
; RUN: opt -O2 -verify-each -disable-output %s
; RUN: opt -O3 -verify-each -disable-output %s

target datalayout = "e-m:e-b:32-p:32:32-i64:32-f32:32-f64:32-a:0:32-n32"

; Loading an i64 and storing its halves: the low half goes to byte 0, the
; high half (bits 32-63) to byte 1.
; CHECK-LABEL: @split(
; CHECK: %[[H:.*]] = lshr i64 %x, 32
define void @split(ptr %p, i64 %x) {
  %lo = trunc i64 %x to i32
  %h = lshr i64 %x, 32
  %hi = trunc i64 %h to i32
  store i32 %lo, ptr %p
  %p1 = getelementptr i32, ptr %p, i32 1
  store i32 %hi, ptr %p1
  ret void
}

; Combining two i32 loads into an i64 (load combining in AggressiveInstCombine
; at -O3) must use 32-bit lanes: (hi << 32) | lo.
define i64 @join(ptr %p) {
  %lo = load i32, ptr %p
  %p1 = getelementptr i32, ptr %p, i32 1
  %hi = load i32, ptr %p1
  %lo64 = zext i32 %lo to i64
  %hi64 = zext i32 %hi to i64
  %sh = shl i64 %hi64, 32
  %r = or i64 %sh, %lo64
  ret i64 %r
}
; RUN: opt -passes=aggressive-instcombine -S %s | FileCheck %s --check-prefix=AIC
; If folded, the result is one i64 load from %p; otherwise the two i32 loads
; stay. Either is correct; a load of i64 from %p + anything else is not.
; AIC-LABEL: @join(
; AIC-NOT: load i64, ptr %p1
```

- [ ] **Step 2: Write** `switch-table.ll`:

```llvm
; Switch-to-lookup-table with 32-bit bytes: the index is in elements, which
; are bytes, so it is not scaled by 4.
; RUN: opt -passes='simplifycfg<switch-to-lookup>' -S %s | FileCheck %s
; RUN: opt -O2 -verify-each -disable-output %s

target datalayout = "e-m:e-b:32-p:32:32-i64:32-f32:32-f64:32-a:0:32-n32"

; CHECK: @switch.table.f = private unnamed_addr constant [4 x i32] [i32 10, i32 20, i32 30, i32 40]
; CHECK-LABEL: @f(
; CHECK-NOT: mul
; CHECK-NOT: shl
; CHECK: load i32, ptr %switch.gep
define i32 @f(i32 %x) {
entry:
  switch i32 %x, label %def [
    i32 0, label %a
    i32 1, label %b
    i32 2, label %c
    i32 3, label %d
  ]
a: br label %ret
b: br label %ret
c: br label %ret
d: br label %ret
def: br label %ret
ret:
  %r = phi i32 [ 10, %a ], [ 20, %b ], [ 30, %c ], [ 40, %d ], [ 0, %def ]
  ret i32 %r
}
```

SimplifyCFG may build the table only with a target that allows it (`TTI::shouldBuildLookupTables`; the default is true). If the generated name differs (`@switch.table.f` vs others), match by `[4 x i32] [i32 10, i32 20, i32 30, i32 40]`.

- [ ] **Step 3: Run both** and triage failures.
Run: `build-sharc/bin/llvm-lit -v llvm/test/Transforms/ByteWidth/store-merge.ll llvm/test/Transforms/ByteWidth/switch-table.ll`

- [ ] **Step 4: Commit.**
```bash
git add llvm/test/Transforms/ByteWidth/store-merge.ll llvm/test/Transforms/ByteWidth/switch-table.ll
git commit -m "[ByteWidth] Test store merging and switch tables with 32-bit bytes"
```

---

### Task 7: Audit table, docs and completion

**Files:**
- Modify: `llvm/docs/SHARCUsage.rst` (new section "Non-8-bit bytes"), `docs/superpowers/specs/2026-10-04-sharc-m3-byte-width-design.md` (§6 and a Status section)

- [ ] **Step 1: Unaffected passes get a test too.** For each pass of spec §4 (SROA, InstCombine, GVN, MemCpyOpt, LoopIdiomRecognize, ConstantFolding, SimplifyLibCalls, LSR) not already fixed or guarded with a test, point to the corpus test that exercises it (the corpus RUN lines name the pass). If none does, add a `b:32` case to that pass's test directory showing correct behaviour.

- [ ] **Step 2: Write the docs section** in `llvm/docs/SHARCUsage.rst`, after "Linking":

```rst
Non-8-bit bytes
===============

Classic SHARC addresses memory in 32-bit words, so its DataLayout will say
``b:32`` (M4): an IR byte is 32 bits, ``i32`` is one byte and ``i64`` two.
Support comes from an open upstream patch series by Sergei Barannikov,
imported on this branch, plus fixes found by a ``b:32`` test corpus in
``llvm/test/Transforms/ByteWidth``.

.. list-table::
   :header-rows: 1

   * - Upstream patch
     - Status
   * - #106536 [DataLayout] Add byte specification
     - imported
   ... (one row per spec §2 commit)

.. list-table::
   :header-rows: 1

   * - Pass
     - Outcome
     - Test
   ... (one row per audited pass: fixed / guarded / unaffected)

Deferred: ... (each guard, with what a fix would need)
```

Fill every row from Tasks 1-6 (no "..." left). Validate: `uvx --from docutils rst2html --report=2 llvm/docs/SHARCUsage.rst > /dev/null`.

- [ ] **Step 3: Run the completion suites.**
```bash
ninja -C build-sharc
build-sharc/bin/llvm-lit -q llvm/test lld/test
build-sharc/unittests/IR/IRTests --gtest_brief=1
build-sharc/unittests/Analysis/AnalysisTests --gtest_brief=1
```
Expected: no failures (unsupported tests for unbuilt targets are fine). Compare the Transforms/Analysis pass counts with Task 1 Step 4: no test that passed before the M3 fixes fails now.

- [ ] **Step 4: Record status** in the spec (§6 deferred items filled; a "Status" section against §7 criteria), then commit:
```bash
git add llvm/docs/SHARCUsage.rst docs/superpowers/specs/2026-10-04-sharc-m3-byte-width-design.md
git commit -m "[SHARC] Document non-8-bit byte support; record M3 status"
```
