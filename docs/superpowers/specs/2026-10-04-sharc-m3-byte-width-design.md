# SHARC M3: non-8-bit bytes in IR and the middle end

Status: design approved in conversation, 2026-10-04
Parent: `2026-10-04-sharc-backend-design.md` (roadmap row M3)
Depends on: nothing; generic LLVM work. M4 (classic SHARC CodeGen) builds on it.

## 1. Goal and scope

Classic SHARC addresses memory in 32-bit words, so its C `char`, `short`
and `int` are all 32 bits and the smallest addressable unit (the IR byte) is
32 bits. M3 makes LLVM IR and the optimizer correct for modules whose
DataLayout says `b:32`, as far as M4 needs:

- import the open upstream stack for non-8-bit bytes (§2);
- test the middle end with a corpus of `b:32` IR shaped like classic-SHARC C,
  and fix or guard the passes that assume 8-bit bytes (§3, §4).

Out of scope: SelectionDAG, GlobalISel, AsmPrinter/MC data emission and debug
info (fixed during M4, in generic commits, when SHARC exercises them);
Clang's `CHAR_BIT`, `sizeof` and string literals (M5); vectorizers beyond
not crashing (SHARC has no vector registers); sanitizers and LTO; byte
widths other than 32 in tests (code stays width-generic where that is free).

Decisions (user, 2026-10-04): import the upstream stack rather than rewrite
it; cover what M4 needs, test-driven, rather than a broad audit; M3 is IR and
middle end, CodeGen fixes belong to M4.

## 2. The upstream stack

Sergei Barannikov's stack (branches `users/s-barannikov/byte/1..7` in
llvm/llvm-project, based on May 2026 `main`; all PRs open):

| # | Commit | PR | Title |
|---|---|---|---|
| 1 | 63fa6c81d677 | #106536 | [DataLayout] Add byte specification |
| 2 | 389835147eab | #106537 | [IR] Make @llvm.memset prototype byte width dependent |
| 3 | 611ae7ec7052 | #106538 | [ValueTracking] Make isBytewiseValue byte width agnostic |
| 4 | 389c7661aa94 | #106539 | [IRBuilder] Add getByteTy and use it in CreatePtrAdd |
| 5 | 966835daf0e3 | (in #106539's branch) | Use DL in ConstantExpr::getPtrAdd() / getInBoundsPtrAdd() |
| 6 | 2e73d7ff5209 | #106540 | [IR] Account for byte width in m_PtrAdd |
| 7 | 3ccd33f9471e | #106541 | [ValueTracking] Add CharWidth argument to getConstantStringInfo (NFC) |
| 8 | 21602592d581 | #106542 | [SimplifyLibCalls] Add initial support for non-8-bit bytes |

Import rules:

- `git cherry-pick -x` in this order onto `sharc`; the author stays Sergei
  Barannikov.
- A probe against today's base (October 2026) showed commit 1 applies
  cleanly and the others conflict in a few files each (intrinsic tables,
  `Constants`, ScalarEvolution, InstCombineCalls, unit tests, one Clang
  AMDGPU file). Conflicts are resolved against today's code and described in
  the commit message ("Conflicts: ...").
- After each commit: build, then its own tests plus `check-llvm-ir` and
  `check-llvm-analysis` pass before the next.
- Parts touching Clang, MLIR or sanitizers are kept as they are (they keep
  the stack coherent) and only compile-checked where the build includes them.
- These are generic commits, kept apart from SHARC target commits.

## 3. Test corpus

`llvm/test/Transforms/ByteWidth/`, generic (no SHARC dependency). Every
file's module has a `b:32` DataLayout (based on the provisional SHARC layout
`e-m:e-p:32:32-i64:32-f64:32-a:0:32-n32` plus `b:32`) and stands for code
Clang will emit for classic SHARC in M5:

| File | Content |
|---|---|
| `strings.ll` | `[N x i32]` strings; `strlen`, `strcmp`, `strchr`, `memcpy` of strings |
| `aggregates.ll` | struct copy, assignment and zero-init (`memcpy`/`memset` lengths in 32-bit bytes) |
| `pointers.ll` | `getelementptr i32`, `ptradd`, `i64` and `i8` GEPs (an `i8` occupies a whole 32-bit byte, so it steps by 1 byte) |
| `loop-idioms.ll` | loops LoopIdiomRecognize turns into `memset`/`memcpy`; LSR-reduced loops |
| `globals.ll` | loads from constant globals and aggregates (ConstantFolding at byte offsets) |
| `sroa.ll` | small struct allocas split by SROA |
| `store-merge.ll` | adjacent stores merged by InstCombine and MemCpyOpt |
| `switch-table.ll` | switch lowering to lookup tables |

Each file has:

1. robustness RUN lines: `opt -O0/-O1/-O2/-O3 -verify-each` (assertions
   build) with no crash and valid IR;
2. FileCheck lines for the facts that matter, computed by hand (a 4-word
   struct copy is `memcpy` of 4 bytes; `strlen` of a 5-character literal
   folds to 5; a GEP over `i32` steps by 1 byte).

## 4. Pass audit

Passes that do byte arithmetic, each tested under `b:32`: SROA, InstCombine
(load/store combining, GEP canonicalization), GVN (VNCoercion), MemCpyOpt,
LoopIdiomRecognize, ConstantFolding, SimplifyLibCalls (covered by the stack),
LSR. More are added when the corpus shows a problem.

For each pass the outcome is one of:

- **fixed:** a generic commit makes it byte-width correct, with a `b:32` test
  in the pass's own test directory;
- **guarded:** fixing is large, so the pass skips modules with
  `DL.getByteWidth() != 8` where it would be wrong; a test shows `b:32` code
  is left alone; the deferred fix is listed in §6;
- **unaffected:** a test shows correct `b:32` behaviour without a change.

Every change also keeps the pass's existing tests green, so 8-bit targets are
unaffected.

## 5. Docs

`llvm/docs/SHARCUsage.rst` gains "Non-8-bit bytes": the imported PRs and
their status, the audit table of §4 with outcomes, and the deferred items.
The LangRef text for `b:` comes with #106536.

## 6. Deferred items

Filled in during M3 with each guarded pass and any other gap found.

## 7. Completion criteria

1. All 8 upstream commits are imported with `-x` and authorship kept; each
   builds and passes its own tests.
2. Every corpus file passes `-O0` to `-O3` with `-verify-each`, and its
   FileCheck facts hold.
3. Every pass in §4 is recorded as fixed, guarded or unaffected, each with a
   test.
4. `check-llvm`, `check-lld` and the SHARC tests pass; 8-bit behaviour is
   unchanged.
5. The "Non-8-bit bytes" section of `SHARCUsage.rst` is written.
