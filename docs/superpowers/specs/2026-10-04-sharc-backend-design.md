# SHARC and SHARC+ backend for LLVM: design

Date: 2026-10-04
Status: design approved in brainstorming; awaiting spec review
Scope: roadmap for the whole backend; detailed design for milestone M1 (MC layer)

## 1. Goals and constraints

- **Goal:** a SHARC target in LLVM covering classic SHARC (ADSP-2136x, 2137x,
  214xx) and SHARC+ (ADSP-2156x, SC58x, 2158x). It is developed downstream on
  the `sharc` branch of this fork and structured so it can later be split into
  upstreamable patch series.
- **Clean room:** only documents Analog Devices publishes on analog.com may be
  used. The vendor toolchain and its files are never used, named, copied or
  quoted. ABI choices (relocations, sections, file layout, later the calling
  convention) are our own designs, documented in `llvm/docs/SHARCUsage.rst`.
- **References:**
  - PGR: SHARC Processor Programming Reference Rev 2.4 (classic core),
    `~/Documents/adsp-2136x_2137x_214xx_pgr_rev2.4.pdf`
  - PRM: SHARC+ Core Programming Reference Rev 1.5,
    `~/Documents/sc58x-2158x-prm.pdf`
  - ADSP-2137x Hardware Reference Rev 2.2 (memory map only)
- **Prior work reused:** digiemu (`~/GitHub/digiemu`) provides clean-room
  encoding tables (`tools/sharcspec/decode_table.json`, 61 forms;
  `compute_table.json`) with documented PRM errata, a Python decoder, a SLEIGH
  spec, an emulator and an execution tracer.
- **Interoperability:** the instruction syntax matches the manuals' algebraic
  syntax. Directives are GNU/LLVM-style, because the vendor directive syntax
  is not in the public manuals. Objects do not need to link with vendor
  objects.
- **Validation resources:** the manuals, digiemu, and real SHARC+ firmware
  (local only; Elektron copyright, never committed or quoted). There is no
  vendor toolchain or hardware.
- **Byte width:** PR llvm/llvm-project#106536 (DataLayout `b:<size>`) is
  still open upstream. M1 and M2 do not depend on it; M3 carries it.

## 2. Roadmap

One target, `SHARC`, in `llvm/lib/Target/SHARC/`. Processors are subtarget
features:

| Feature | Meaning | CPUs |
|---|---|---|
| (base) | 48-bit ISA, SIMD, 40-bit data registers | `adsp-2136x`, `adsp-2137x` |
| `visa` | 16/32/48-bit encodings in short-word space | `adsp-214xx` |
| `sharcplus` (implies `visa`, `byte-addr`, `fp64`) | Types 3d/4d/7d/14d/26a, MODE1STK replacing LIRPTL, Type 12a loop-mode bit, imm23pc | `adsp-2156x`, `adsp-sc58x`, `adsp-2158x` |
| `byte-addr` | Byte-space access modifiers (BW/BWSE/SW/SWSE/EX), B2W/W2B | SHARC+ |
| `fp64` | Double precision on register pairs | SHARC+ |

Milestones. Each one gets its own spec, plan and patch series.

| | Milestone | Depends on |
|---|---|---|
| M1 | MC layer: TargetInfo, registers, formats, AsmParser, InstPrinter, disassembler, ELF writer and relocations, loop-aware sizing, checker, A+C validation | none |
| M2 | Linking: `lld/ELF/Arch/SHARC.cpp`, address-space mapping, default linker script, execution tests in digiemu | M1 |
| M3 | Non-8-bit bytes: #106536 plus the follow-ups 32-bit bytes need in IR, SelectionDAG, MC and Clang. Generic, kept separate from the target | none |
| M4 | CodeGen for classic SHARC (32-bit bytes): clean-room calling convention, ISel, frame lowering, delayed-branch filling, hardware loops; later SIMD and multifunction packing | M1, M3 |
| M5 | Clang target and driver; `char`/`short`/`int` are 32 bits on classic | M3, M4 |
| M6 | SHARC+ byte-addressing CodeGen (8-bit-byte DataLayout variant, B2W/W2B, fp64) | M4, M5 |

M1 and M2 can proceed in parallel with M3.

Commits on `sharc` are kept in three kinds so the history can be split
later: generic changes (the M3 stack and other core fixes), target code
(`Target/SHARC/**`, `lld/ELF/Arch/SHARC.cpp`, docs), and tests. Validation
tooling lives in digiemu. LLVM never depends on it at build time.

## 3. ISA facts that drive the design

Sources are cited as PGR/PRM page (physical PDF page).

1. Classic code is all 48-bit ISA in normal-word (NW) space. VISA (214xx and
   SHARC+) mixes 16-, 32- and 48-bit instructions in short-word (SW) space.
   Execution switches between them only by branching into the other space,
   and reset starts in ISA (PGR p123–136; PRM p49–50, p107).
2. The same bits decode differently in the two spaces. Example: Type 2c
   (`1100…`) collides with Type 10a; Type 10 is illegal in VISA (PGR p226,
   p443).
3. VISA instructions are left-aligned in a 48-bit window. The length comes
   from bits 47–40, 36–34 and 22–16 (the 32-bit "b" forms carry `0111111` in
   bits 22–16) (PGR p446–454).
4. Hardware loops: the last 4 instructions of a loop (214xx) or the last 11
   (SHARC+) must be 48-bit encodings; if the loop is shorter, all of it must
   be. SHARC+ Type 12a bit 23 selects the loop mode (`(F)`) (PGR p172–173;
   PRM p137, p146, p378).
5. Data hazards are interlocked, so the assembler never inserts NOPs.
   Delayed branches have 2 slots on both families (PGR p123; PRM p107, p122).
6. NW, SW and LW address spaces alias the same RAM. On SHARC+, byte space is
   selected by memory region, not a mode bit. Access size comes from the
   opcode modifier, B2W/W2B convert by roughly ÷4/×4, and byte-space data is
   little-endian (PGR p296–301; PRM p188–201, p222, p780).
7. `EM_SHARC = 133` already exists in `llvm/include/llvm/BinaryFormat/ELF.h`.

## 4. M1 design

### 4.1 Instruction model

- **Form opcodes**, one per Type and variant (`T1a`, `T4b_BW`, `T8a_rel`, …),
  are generated from `decode_table.json`. Forms marked undocumented there
  (`Type*p_undoc*`, observed only in firmware) are excluded under the
  clean-room rule. Their operands are the fixed fields;
  the compute slot is a nested `MCOperand::createInst`.
- **Compute opcodes** are generated from `compute_table.json`. They have their
  own 23-bit encodings and decoder namespace. Short compute (Type 2c, 12-bit)
  is a separate subset table.
- **Encoding:** TableGen encodes each form. A custom operand encoder places
  the nested compute encoding into the compute field.
- **Bit numbering:** 32- and 16-bit forms are defined left-aligned in their
  48-bit window, so the bit numbers in the `.td` match the manuals.
- **Register classes** come from the 7-bit Ureg group codes: R/F, I, M, L, B,
  S/SF, the misc group, the system registers, I1/I2 and M1/M2 halves, and
  UREGXDAG1/2. They also include the restricted multifunction classes
  (R0–3, R4–7, R8–11, R12–15 and F equivalents) and the SHARC+ register pairs
  (F1:0 … F15:14, SF pairs).
- **Features:** classic-only and SHARC+-only encodings carry predicates. On
  SHARC+, MODE1STK reuses LIRPTL's code (0x7D).

### 4.2 ISA vs VISA sections

- The processor-specific section flag `SHF_SHARC_VISA` (in `SHF_MASKPROC`)
  marks VISA code. It is set by the `v` flag in `.section …,"axv"`, or by the
  `.visa`/`.isa` directives for the current section. Code sections are ISA
  unless marked; `-mattr=+visa-section` makes VISA the default for unmarked
  code sections (and marks them). (Changed during M1 Task 12 from "VISA by
  default on +visa targets", which silently changed the meaning of existing
  ISA code.) There are no mapping symbols, because placement determines the
  mode and the two can't mix within one section.
- The AsmParser selects forms by section mode, and the disassembler reads the
  flag. `llvm-objdump` uses the section flag; raw disassembly uses
  `-mattr=+visa` (or its absence).
- Errors: an ISA-only form in a VISA section (e.g. Type 10), or the `v` flag
  on a target without `visa`.

### 4.3 ELF addressing and relocations

- **ELF addresses are byte addresses.** A relocation computes the core address
  as `(S + A) / unit`, with the unit fixed by the relocation type: NW 32-bit
  data 4, SW 2, LW 8, SHARC+ byte space 1.
- For data, NW×4 = SW×2 = LW×8 is the same physical byte, and on SHARC+ the
  ELF data address equals the real byte-space address.
- **Resolved (M1 Task 1):** 48-bit NW words pack densely, so the mapping is
  affine per memory block (PGR p300, PRM p228–229). ELF addresses are the
  hardware byte-space address on SHARC+ and `2 × SW` on classic SHARC. Every
  word-space field is `Wbase + (S + A − Bbase) / unit` for the memory block
  containing `S + A`, with block bases from a per-CPU table in LLD (M2). See
  `llvm/docs/SHARCUsage.rst`, "Address units". This replaces the earlier
  "data address = NW×4 = real byte address" assumption, which holds only up
  to a per-region offset on SHARC+ (PRM Table 6-4).
- **Byte order in the file:** `ELFDATA2LSB`. An instruction is stored as
  16-bit parcels, most significant parcel first, and each parcel is
  little-endian. This matches the layout observed in real SHARC+ images
  (digiemu `tools/sharcspec/sharc_decode.py`). Data is plain little-endian
  (a 32-bit word is an ordinary LE 32-bit value), as digiemu's loader
  analysis also finds.
- **Relocations** (our own numbering, documented in `SHARCUsage.rst`):
  - absolute: `R_SHARC_ADDR24_{NW,SW}`, `R_SHARC_ADDR32_{NW,SW,LW,BYTE}`,
    `R_SHARC_DATA32`
  - PC-relative: `R_SHARC_PCREL24_{ISA,VISA}`, `R_SHARC_PCREL23_{ISA,VISA}`
    (SHARC+ Type 12a), `R_SHARC_PCREL6_{ISA,VISA}`. The PC-relative unit is a
    48-bit word in ISA and a 16-bit parcel in VISA.
  - small immediates: `R_SHARC_IMM16`, `R_SHARC_IMM7`, `R_SHARC_IMM6`

### 4.4 AsmParser

- **Hand-written,** like Hexagon's, because algebraic syntax has no leading
  mnemonic for AsmMatcher. Input is case-insensitive.
- **Steps:**
  1. Split the statement on top-level commas into slots: condition, compute,
     DM move, PM move, branch, modifiers (`(DB)`, `(LA)`, `(CI)`, `(LR)`,
     `(LW)`, `(BW)`, `(BWSE)`, `(SW)`, `(SWSE)`, `(EX)`, `(NW)`, `(F)`), and
     `ELSE` compute.
  2. Parse each slot with recursive descent. The compute grammar is generated
     from `compute_table.json`.
  3. Choose the smallest form valid for the filled slots, the features and the
     section mode. On a mismatch, report which slot combination is illegal.
- **Directives:** GNU/LLVM ones (`.section`, `.word`, `.globl`, `.p2align`,
  …) plus `.isa` and `.visa`. Any SHARC data directive needed for 48-bit
  words is added only when a test requires it.

### 4.5 Sizing pass and checker

- **Relaxation:** VISA instructions go into relaxable fragments.
  `relaxInstruction` widens forms whose label operand doesn't fit
  (imm6pc → pcrel24, imm16 → imm32).
- **Loop-end widening:**
  1. On `DO lbl UNTIL …`, the parser records `lbl` together with the DO
     instruction's fragment.
  2. When `lbl` is defined (it labels the loop's last instruction), the
     streamer widens that instruction and the preceding N−1 to 48-bit forms
     (N = 4 on `visa`, 11 on `sharcplus`). A shorter loop is widened in full.
  3. Widening only grows sizes, so layout converges.
- **SHARC+ loop mode:** at the same point, set the DO's `(F)` bit by checking
  the last 11 instructions for branches or IDLE, unless the source gave it
  explicitly.
- **Checker** (like HexagonMCChecker), which reports errors for:
  - a branch in either `(DB)` delay slot, except for mutually exclusive
    conditions;
  - JUMP/CALL in the last 3 instructions of a loop;
  - both memory moves writing the same register;
  - a loop end label that isn't defined in the DO's section.

  SHARC+ branch-predictor guidance (PRM p118) becomes warnings that are off
  by default.

### 4.6 Disassembler and printer

- **Disassembler:** read a 48-bit window, zero-padded at the end of a section.
  - ISA mode uses `DecoderTable48`.
  - VISA mode works out the length from bits 47–40, 36–34 and 22–16, then
    uses `DecoderTable16`, `DecoderTable32` or `DecoderTable48`, with feature
    predicates for classic vs SHARC+ discriminators.
  - The compute field is decoded with the compute table.
- **Printer:** prints the PGR's canonical uppercase algebraic form. Printed
  output must reassemble to identical bytes.

## 5. M1 validation

### 5.1 Generator (approach A)

- `digiemu/tools/sharcspec/gen_llvm_td.py` emits `SHARCInstrFormats.td`,
  `SHARCInstrInfo.td` (forms) and `SHARCInstrCompute.td`.
- The output is committed once, with a provenance header, and is maintained
  by hand afterwards.
- `--diff` compares fresh generator output with the committed `.td`, so later
  table corrections arrive as reviewable diffs.

### 5.2 Blind transcription (approach C)

- A fresh agent in an isolated directory with only the PGR and PRM (no
  digiemu, no `sharc` branch) produces `blind_forms.json` and
  `blind_compute.json` in a neutral schema (form, width, feature, mask, value,
  fields). Forms and compute get separate passes.
- `compare_td.py` normalizes `llvm-tblgen --dump-json` output from the `.td`
  and diffs it field by field against the blind tables.
- Each discrepancy is settled by reading the manual figure by eye and recorded
  in the errata appendix of `SHARCUsage.rst`: manual, page, what it says, what
  we use, and why.
- Finally, check whether the blind pass independently found the 10 PRM errata
  digiemu already lists. Any it missed lower confidence in its agreements.

### 5.3 Lit tests (committed)

- `llvm/test/MC/SHARC/`:
  - encodings per Type group under `classic`, `+visa` and `+sharcplus`;
  - `invalid-*.s` for each checker rule and for illegal slot combinations;
  - `loop-widen-214xx.s`, `loop-widen-sharcplus.s`, `loop-mode-f.s`;
  - `relax-*.s`, `reloc-*.s`, `section-visa-flag.s`.
- `llvm/test/MC/Disassembler/SHARC/`: ISA and VISA files for each width, and
  collision cases (2c vs 10a by mode).
- Expected encodings come from the resolved A+C tables and are spot-checked
  against the manuals' examples. They are never captured from LLVM's own
  output.

### 5.4 Local differential and corpus checks (digiemu)

- **Encoding sweep:** for each form, enumerate the small fields exhaustively
  and sample the wide immediates. `llvm-mc --disassemble -show-inst`,
  `sharc_decode.py` and SLEIGH (via pypcode) must agree on form and field
  values. LLVM disassembly must reassemble to identical bytes.
- **Firmware corpus:** every instruction word that digiemu's trace shows
  executing in the SHARC+ firmware must disassemble in LLVM and reassemble to
  identical bytes. The same applies to any classic-SHARC image digiemu has.
  Only aggregate counts and form coverage are recorded; firmware bytes and
  listings stay local.

## 6. M1 completion criteria

- Every form and every compute opcode has a lit test.
- A vs C has zero unresolved discrepancies, and the errata appendix is
  written.
- The encoding sweep shows three-way agreement.
- The firmware corpus decodes 100%, except words that digiemu classifies as
  undocumented forms (counted and reported separately), and round-trips
  byte-identically.
- The 48-bit code addressing item (4.3) is resolved and documented in
  `SHARCUsage.rst`.

### 6.1 Status (2026-10-04)

M1 is implemented. Against the criteria:

- **Met:** every one of the 378 forms and computations has a lit test.
- **Met:** the encoding sweep agrees with digiemu's decoder and SLEIGH in all
  four configurations, with no failures.
- **Met:** 4.3 is resolved and documented.
- **Met, with open items:** the A-vs-C errata appendix is written. Two
  questions remain open: the Type 19a SHARC+ `sc`/`w` fields, which overlap
  the bitrev encoding and are not supported, and the Type 16a `sl`/`by` bits.
- **Not fully met:** the SHARC+ firmware corpus round-trips 21,493 of 21,494
  documented-form words. The remaining one is Type 18a with bit 36 set; it is
  decoded with a warning because the manuals do not define the bit.
- **Not met:** the classic-SHARC corpus. The Digitakt mk1 DSP section is not
  in the SHARC+ boot-stream format, so digiemu's loader needs a 214xx
  boot-stream parser before this can run.

## 7. Out of scope for M1

Linking and LLD (M2); DataLayout byte width (M3); CodeGen, calling convention
and Clang (M4–M6); vendor directive syntax; link compatibility with vendor
objects; DWARF beyond what MC emits by default.
