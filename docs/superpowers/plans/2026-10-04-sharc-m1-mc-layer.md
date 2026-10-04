# SHARC M1 (MC layer) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** an LLVM MC layer for SHARC and SHARC+ (`llvm-mc` assembles and
disassembles, `llvm-objdump` and `llvm-readobj` understand SHARC ELF),
validated three ways: two independent readings of the manuals, digiemu's
decoder and SLEIGH spec, and real firmware bytes.

**Architecture:** one experimental target, `SHARC`, with subtarget features
`visa`, `sharcplus`, `byte-addr` and `fp64`.
- **Instruction encodings** are generated once from digiemu's clean-room
  tables into TableGen, then maintained by hand.
- **Instruction model:** each instruction "form" (Type 1a, 2c, …) is one
  opcode, and the compute part is a nested `MCInst` operand.
- **Slot table:** a hand-written per-form "slot" table (`SHARCFormSlots.def`)
  drives both the printer and the hand-written algebraic AsmParser.
- **Mixed widths:** VISA's 16/32/48-bit instructions use per-width decoder
  tables and a length classifier generated from the same data.

**Tech Stack:** C++17 / LLVM MC, TableGen, lit / FileCheck, CMake + Ninja.
Python 3 with `uv` and pytest for the digiemu tooling. pymupdf only in the
blind-transcription task.

**Spec:** `docs/superpowers/specs/2026-10-04-sharc-backend-design.md` (read it
first; this plan implements its section 4–6, milestone M1).

## Global Constraints

- **Clean room:** use only the public ADI manuals listed in the spec. Never
  use, name, copy or quote the vendor toolchain or its files, in code,
  comments, tests or commit messages.
- **Firmware** (Elektron) and anything derived from it (bytes, listings,
  per-address output) is never committed, quoted or pasted into LLVM or
  digiemu files. Only aggregate counts may be recorded.
- **Undocumented forms** (`Type*p_undoc*` in `decode_table.json`) are never
  part of LLVM.
- **Lit test expectations** come from the resolved tables via
  `llvm_vectors.py` (Task 4), spot-checked against the manual. They are
  never captured from LLVM's own output.
- **File layout:** each instruction is stored as 16-bit parcels, most
  significant parcel first, and each parcel is little-endian. The ELF is
  `ELFDATA2LSB`, `EM_SHARC` (133), ELFCLASS32.
- **Target name** `SHARC`; triple arch `sharc`. CPUs `adsp-2136x`,
  `adsp-2137x`, `adsp-214xx`, `adsp-2156x`, `adsp-sc58x`, `adsp-2158x`.
- **Commit messages** on `sharc` start with `[SHARC]`, or `[SHARC][digiemu]`
  for commits in the digiemu repo. digiemu commits are made only when the
  user asks (digiemu's CLAUDE.md rule), so a "Commit" step in digiemu means:
  stage the change and report it for the user to commit.
- **Mirroring Xtensa:** when a step says to mirror Xtensa, copy the *current*
  signature from the named Xtensa file rather than from this plan; MC APIs
  change often.

## Build and test commands (used throughout)

```bash
# One-time configure (from /home/kate/GitHub/llvm-project)
cmake -G Ninja -S llvm -B build-sharc \
  -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=SHARC \
  -DLLVM_CCACHE_BUILD=ON
# Build the tools the tests use
ninja -C build-sharc llvm-mc llvm-objdump llvm-readobj llvm-tblgen yaml2obj FileCheck not count
# Run SHARC lit tests
build-sharc/bin/llvm-lit -v llvm/test/MC/SHARC llvm/test/MC/Disassembler/SHARC
```

digiemu: `cd ~/GitHub/digiemu && uv run --with pytest python -m pytest tests -q -k sharcspec`

## File structure

LLVM (`llvm/lib/Target/SHARC/`):

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | TableGen and libraries |
| `SHARC.td` | top-level: features, processors, includes |
| `SHARCRegisterInfo.td` | registers and classes from Ureg group codes |
| `SHARCInstrFormats.td` | **generated once**: base classes per width |
| `SHARCInstrInfo.td` | **generated once**: one def per form |
| `SHARCInstrCompute.td` | **generated once**: compute, short compute, shift-imm, multifunction |
| `SHARCOperands.td` | operand classes (cond, compute, immediates) |
| `SHARCFormSlots.def` | hand-written: form → slots and field operand indices |
| `SHARCFormLengths.def` | **generated once**: VISA length classifier data |
| `SHARCComputeSyntax.def` | **generated once**: compute opcode → syntax pattern |
| `SHARCWideForms.def` | **generated once**: narrow form → 48-bit form |
| `SHARCTargetMachine.{h,cpp}`, `SHARC.h` | stub TargetMachine (no CodeGen yet) |
| `TargetInfo/SHARCTargetInfo.{h,cpp}` | target registration |
| `MCTargetDesc/SHARCMCTargetDesc.{h,cpp}` | MC registration |
| `MCTargetDesc/SHARCMCAsmInfo.{h,cpp}` | comment and label syntax |
| `MCTargetDesc/SHARCParcels.h` | parcel byte layout helpers |
| `MCTargetDesc/SHARCInstPrinter.{h,cpp}` | slot-driven printer |
| `MCTargetDesc/SHARCMCCodeEmitter.cpp` | parcel layout, nested compute encoding, fixups |
| `MCTargetDesc/SHARCFixupKinds.h` | fixup kinds and their bit segments |
| `MCTargetDesc/SHARCAsmBackend.cpp` | applyFixup, relaxation, NOPs |
| `MCTargetDesc/SHARCELFObjectWriter.cpp` | relocation types |
| `MCTargetDesc/SHARCTargetStreamer.{h,cpp}` | `.isa`/`.visa`, section flag, e_flags |
| `Disassembler/SHARCDisassembler.cpp` | ISA/VISA length dispatch |
| `AsmParser/SHARCAsmParser.cpp` | slot splitter, form selection, loop buffer |
| `AsmParser/SHARCComputeParser.{h,cpp}` | compute-pattern matcher |
| `AsmParser/SHARCLoopTracker.{h,cpp}` | DO…UNTIL widening and loop mode |
| `AsmParser/SHARCChecker.{h,cpp}` | delay-slot, loop-end and double-write rules |

Other LLVM files:
- `llvm/include/llvm/TargetParser/Triple.h`, `llvm/lib/TargetParser/Triple.cpp`
  (arch `sharc`)
- `llvm/include/llvm/BinaryFormat/ELF.h`, `ELFRelocs/SHARC.def`
  (relocations, e_flags, `SHF_SHARC_VISA`)
- `llvm/lib/Object/ELF.cpp`, `llvm/include/llvm/Object/ELFObjectFile.h`,
  `llvm/lib/Object/ELFObjectFile.cpp` (names, arch, features from e_flags)
- `llvm/CMakeLists.txt` (experimental target list)
- `llvm/docs/SHARCUsage.rst` (ABI and errata)
- `llvm/test/MC/SHARC/**`, `llvm/test/MC/Disassembler/SHARC/**`,
  `llvm/test/tools/llvm-readobj/ELF/sharc-*.test`,
  `llvm/unittests/TargetParser/TripleTest.cpp`

digiemu (`~/GitHub/digiemu/tools/sharcspec/`):

| File | Responsibility |
|---|---|
| `gen_llvm_td.py` | emits the generated `.td` and `.def` files; has a `--diff` mode |
| `llvm_vectors.py` | form + field values → 48-bit word → parcel bytes, for lit expectations |
| `blind/` (separate directory, see Task 15) | blind transcription output |
| `compare_td.py` | `llvm-tblgen --dump-json` vs the blind tables |
| `../sharc_llvm_sweep.py` | three-way encoding sweep |
| `../sharc_llvm_corpus.py` | firmware corpus round trip (local only) |
| `tests/test_sharcspec_llvm.py` | pytest for the generator and vectors |

---

### Task 1: Settle code and data addressing (blocks Tasks 11 and 18)

This is research that ends in a written decision. It writes no code; its
output is `SHARCUsage.rst` sections that Task 11 implements.

**Files:**
- Create: `llvm/docs/SHARCUsage.rst`
- Modify: `llvm/docs/CompilerWriterInfo.rst` (add the PGR and PRM under
  "Analog Devices")

**Interfaces:**
- Produces: a table in `SHARCUsage.rst`, "Address units", giving for each
  relocation the formula from ELF byte address to field value. Task 11
  implements exactly that table.

- [ ] **Step 1: Read the memory chapters.** Read PGR p296–301 (physical PDF
  pages; "Memory" chapter: NW/SW/LW aliasing, 48-bit column rotation) and PRM
  p188–201 and p222 (byte space, B2W/W2B). Extract the text with
  `pdftotext -f 290 -l 305 -layout ~/Documents/adsp-2136x_2137x_214xx_pgr_rev2.4.pdf <scratchpad>/pgr-mem.txt`.

- [ ] **Step 2: Work out the 48-bit code mapping.** Answer the following,
  with page citations:
  1. For a 48-bit NW instruction address `n` in a block with NW base `b48`,
     what is the short-word address of its first parcel? (Expected shape:
     `sw = swbase + 3*(n - b48)`, possibly with the rotation rule. Write down
     what the PGR actually says.)
  2. Is that relationship affine per block? If so, an ISA absolute
     relocation can be computed as `nw = b48 + ((S + A)/2 - swbase)/3`, with
     the block bases taken from the linker script (M2).
  3. If it is not affine, record the fallback the spec names: a
     per-output-section address unit carried in the SHARC-specific section
     attribute.

- [ ] **Step 3: Check the image byte layout of data words.** In digiemu,
  read `tools/sharcldr.py` and `docs/SHARC-ADDRESS-MAP.md` (code and
  findings only, not firmware bytes). Determine how 32-bit data words are
  ordered in loader images: two LE parcels, MSW first, or a little-endian
  32-bit word. Record the answer as an aggregate statement, with no bytes.

- [ ] **Step 4: Write `SHARCUsage.rst`** with these sections: "Processors and
  features" (the feature table from spec §2), "Object file layout" (the
  parcel rule, `ELFDATA2LSB`, and the data-word answer from Step 3),
  "Address units" (Step 2's table), "Relocations" (filled in by Task 11),
  and "Manual errata" (filled in by Task 15). Mark the Relocations and
  Manual errata sections as in progress. Start the file with the standard
  LLVM docs header:

```rst
=============================
User Guide for SHARC Target
=============================

.. contents::
   :local:

Introduction
============

The SHARC target supports Analog Devices SHARC (ADSP-2136x, ADSP-2137x,
ADSP-214xx) and SHARC+ (ADSP-2156x, ADSP-SC58x, ADSP-2158x) processors. All
encodings and ABI conventions here derive from the publicly available
*SHARC Processor Programming Reference* (Rev 2.4) and *SHARC+ Core
Programming Reference* (Rev 1.5).
```

- [ ] **Step 5: Build the docs.** Run
  `ninja -C build-sharc docs-llvm-html` if Sphinx is configured; otherwise run
  `python3 -m docutils llvm/docs/SHARCUsage.rst /dev/null`. Expected: no
  errors.

- [ ] **Step 6: Commit**

```bash
git add llvm/docs/SHARCUsage.rst llvm/docs/CompilerWriterInfo.rst
git commit -m "[SHARC] Document object layout and address units"
```

---

### Task 2: Triple, ELF constants and Object support

**Files:**
- Modify: `llvm/include/llvm/TargetParser/Triple.h` (ArchType enum, next to `xtensa`)
- Modify: `llvm/lib/TargetParser/Triple.cpp` (every `case xtensa:` / `Triple::xtensa` site: lines ~164, 374, 527, 682, 1010, 1776, 1887, 1971, 2111, 2257, 2507)
- Create: `llvm/include/llvm/BinaryFormat/ELFRelocs/SHARC.def`
- Modify: `llvm/include/llvm/BinaryFormat/ELF.h` (after the Xtensa reloc include ~line 1105)
- Modify: `llvm/lib/Object/ELF.cpp` (~line 179)
- Modify: `llvm/include/llvm/Object/ELFObjectFile.h` (~lines 1324, 1456)
- Modify: `llvm/lib/Object/ELFObjectFile.cpp` (`getFeatures`)
- Test: `llvm/unittests/TargetParser/TripleTest.cpp`
- Test: `llvm/test/tools/llvm-readobj/ELF/sharc-header.test`
- Test: `llvm/test/tools/llvm-readobj/ELF/reloc-types-sharc.test`

**Interfaces:**
- Produces: `Triple::sharc`; `ELF::R_SHARC_*` (numbers below);
  `ELF::EF_SHARC_MACH_{2136X=1,2137X=2,214XX=3,SHARCPLUS=4}` with
  `EF_SHARC_MACH = 0xf`; `ELF::SHF_SHARC_VISA = 0x10000000`.

- [ ] **Step 1: Write the failing Triple unittest.** Append to
  `TripleTest.cpp`:

```cpp
TEST(TripleTest, SHARC) {
  Triple T("sharc-unknown-elf");
  EXPECT_EQ(Triple::sharc, T.getArch());
  EXPECT_EQ("sharc", Triple::getArchTypeName(Triple::sharc));
  EXPECT_EQ("sharc", Triple::getArchTypePrefix(Triple::sharc));
  EXPECT_EQ(Triple::ELF, T.getObjectFormat());
  EXPECT_TRUE(T.isArch32Bit());
  EXPECT_TRUE(T.isLittleEndian());
  EXPECT_EQ(Triple::sharc, T.get32BitArchVariant().getArch());
  EXPECT_EQ(Triple::UnknownArch, T.get64BitArchVariant().getArch());
  EXPECT_EQ(Triple::UnknownArch, T.getBigEndianArchVariant().getArch());
}
```

- [ ] **Step 2: Run it and watch it fail.**
  Run: `ninja -C build-sharc TargetParserTests && build-sharc/unittests/TargetParser/TargetParserTests --gtest_filter=TripleTest.SHARC`
  Expected: compile error, `sharc` is not a member of `Triple`.

- [ ] **Step 3: Add the arch.** In `Triple.h`, add
  `sharc,          // Analog Devices: SHARC, SHARC+` after `xtensa`. In
  `Triple.cpp`, add `sharc` beside `xtensa` in each switch:
  - name and prefix: `return "sharc";`
  - `parseArch` and `parseArch(StringRef)`: `.Case("sharc", Triple::sharc)`
  - `getDefaultFormat`: return `Triple::ELF`
  - `getArchPointerBitWidth`: 32
  - `get32BitArchVariant`: itself
  - `get64BitArchVariant`: `UnknownArch`
  - `getBigEndianArchVariant`: `UnknownArch`
  - `isLittleEndian`: true
  - `getDefaultExceptionHandling`: same as xtensa

- [ ] **Step 4: Run the unittest.** Expected: PASS. Then run the whole
  `TargetParserTests` binary and expect PASS, since some tests iterate over
  all arches.

- [ ] **Step 5: Add the ELF constants.** Create `ELFRelocs/SHARC.def`:

```cpp
#ifndef ELF_RELOC
#error "ELF_RELOC must be defined"
#endif

// Clean-room relocation set; see llvm/docs/SHARCUsage.rst "Relocations".
ELF_RELOC(R_SHARC_NONE,          0)
ELF_RELOC(R_SHARC_DATA32,        1)
ELF_RELOC(R_SHARC_ADDR24_NW,     2)
ELF_RELOC(R_SHARC_ADDR24_SW,     3)
ELF_RELOC(R_SHARC_ADDR32_NW,     4)
ELF_RELOC(R_SHARC_ADDR32_SW,     5)
ELF_RELOC(R_SHARC_ADDR32_LW,     6)
ELF_RELOC(R_SHARC_ADDR32_BYTE,   7)
ELF_RELOC(R_SHARC_PCREL24_ISA,   8)
ELF_RELOC(R_SHARC_PCREL24_VISA,  9)
ELF_RELOC(R_SHARC_PCREL23_ISA,  10)
ELF_RELOC(R_SHARC_PCREL23_VISA, 11)
ELF_RELOC(R_SHARC_PCREL6_ISA,   12)
ELF_RELOC(R_SHARC_PCREL6_VISA,  13)
ELF_RELOC(R_SHARC_IMM16,        14)
ELF_RELOC(R_SHARC_IMM7,         15)
ELF_RELOC(R_SHARC_IMM6,         16)
```

  In `ELF.h`, after the Xtensa block:

```cpp
// SHARC specific e_flags
enum : unsigned {
  EF_SHARC_MACH = 0x0000000f,
  EF_SHARC_MACH_NONE = 0,
  EF_SHARC_MACH_2136X = 1,
  EF_SHARC_MACH_2137X = 2,
  EF_SHARC_MACH_214XX = 3,
  EF_SHARC_MACH_SHARCPLUS = 4,
};

// SHARC specific section flags
enum : unsigned {
  // Code in this section is linked into short-word space and decodes as VISA.
  SHF_SHARC_VISA = 0x10000000,
};

// ELF Relocation types for SHARC
enum {
#include "ELFRelocs/SHARC.def"
};
```

  `EM_SHARC = 133` already exists (`ELF.h:247`); don't add it again.

- [ ] **Step 6: Wire up Object.**
  - `ELF.cpp` `getELFRelocationTypeName`: add a `case ELF::EM_SHARC:` that
    includes `SHARC.def`.
  - `ELFObjectFile.h` `getFileFormatName`: `case ELF::EM_SHARC: return "elf32-sharc";`
  - `getArch`: `case ELF::EM_SHARC: return Triple::sharc;`
  - `ELFObjectFile.cpp` `getFeatures()`: add a `getSHARCFeatures()` that maps
    `EF_SHARC_MACH`:
    - 3 → `+visa`
    - 4 → `+sharcplus,+visa,+byte-addr,+fp64`

- [ ] **Step 7: Write the failing readobj tests.**
  `llvm/test/tools/llvm-readobj/ELF/sharc-header.test`:

```yaml
# RUN: yaml2obj %s -o %t.o
# RUN: llvm-readobj -h %t.o | FileCheck %s
# CHECK: Format: elf32-sharc
# CHECK: Arch: sharc
# CHECK: Machine: EM_SHARC (0x85)
--- !ELF
FileHeader:
  Class:   ELFCLASS32
  Data:    ELFDATA2LSB
  Type:    ET_REL
  Machine: EM_SHARC
```

  `reloc-types-sharc.test`: copy the structure of
  `reloc-types-xtensa.test` in the same directory, listing all 17
  `R_SHARC_*` names with their numbers.

- [ ] **Step 8: Run them.**
  Run: `ninja -C build-sharc llvm-readobj yaml2obj && build-sharc/bin/llvm-lit -v llvm/test/tools/llvm-readobj/ELF/sharc-header.test llvm/test/tools/llvm-readobj/ELF/reloc-types-sharc.test`
  Expected: PASS. If yaml2obj rejects the `R_SHARC_*` names, add `SHARC.def`
  to `llvm/lib/ObjectYAML/ELFYAML.cpp` where `Xtensa.def` is included.

- [ ] **Step 9: Commit**

```bash
git add -A llvm/include/llvm/TargetParser llvm/lib/TargetParser llvm/include/llvm/BinaryFormat \
  llvm/lib/Object llvm/include/llvm/Object llvm/lib/ObjectYAML llvm/unittests/TargetParser \
  llvm/test/tools/llvm-readobj/ELF/sharc-header.test llvm/test/tools/llvm-readobj/ELF/reloc-types-sharc.test
git commit -m "[SHARC] Add sharc triple, ELF relocations, e_flags and section flag"
```

---

### Task 3: Target skeleton with registers and features

**Files:**
- Modify: `llvm/CMakeLists.txt:588` (add `SHARC` to `LLVM_ALL_EXPERIMENTAL_TARGETS`)
- Create: `llvm/lib/Target/SHARC/CMakeLists.txt`, `SHARC.td`, `SHARCRegisterInfo.td`, `SHARC.h`,
  `SHARCTargetMachine.{h,cpp}`
- Create: `TargetInfo/{CMakeLists.txt,SHARCTargetInfo.h,SHARCTargetInfo.cpp}`
- Create: `MCTargetDesc/{CMakeLists.txt,SHARCMCTargetDesc.h,SHARCMCTargetDesc.cpp,SHARCMCAsmInfo.h,SHARCMCAsmInfo.cpp}`
- Create: `SHARCInstrInfo.td` holding only `def NOP_placeholder` (replaced in Task 6)
- Test: `llvm/test/MC/SHARC/lit.local.cfg`, `llvm/test/MC/SHARC/cpus.s`

**Interfaces:**
- Produces: `getTheSHARCTarget()`; namespace `SHARC` registers `R0..R15`
  (AltName `F0..F15`), `S0..S15` (`SF0..SF15`), `I0..I15`, `M0..M15`,
  `L0..L15`, `B0..B15`, the misc and system registers named in spec §4.1;
  register classes `RFREG`, `SREG`, `I1REG`, `I2REG`, `M1REG`, `M2REG`,
  `UREG`, `RXMREG`, `RYMREG`, `RXAREG`, `RYAREG`, `DBLREG`; and the feature
  names `FeatureVISA`, `FeatureSHARCPlus`, `FeatureByteAddr`, `FeatureFP64`.
- The Ureg encoding of each register is its `HWEncoding`, a 7-bit code
  (group in bits 6–4).

- [ ] **Step 1: Write the failing test.** `llvm/test/MC/SHARC/lit.local.cfg`:

```python
if not "SHARC" in config.root.targets:
    config.unsupported = True
```

  `llvm/test/MC/SHARC/cpus.s`:

```asm
# RUN: llvm-mc -triple=sharc -mcpu=help %s -o /dev/null 2>&1 | FileCheck %s
# CHECK: adsp-2136x
# CHECK: adsp-2137x
# CHECK: adsp-214xx
# CHECK: adsp-2156x
# CHECK: adsp-2158x
# CHECK: adsp-sc58x
# CHECK: byte-addr
# CHECK: fp64
# CHECK: sharcplus
# CHECK: visa
```

- [ ] **Step 2: Configure and watch it fail.** Run the configure command
  above. Expected: CMake error, unknown target SHARC, or the lit test
  UNSUPPORTED. Either counts as failing.

- [ ] **Step 3: Write `SHARC.td`.**

```tablegen
include "llvm/Target/Target.td"

def FeatureVISA : SubtargetFeature<"visa", "HasVISA", "true",
    "16/32/48-bit VISA encodings in short-word space">;
def FeatureByteAddr : SubtargetFeature<"byte-addr", "HasByteAddr", "true",
    "Byte address space access modifiers and B2W/W2B">;
def FeatureFP64 : SubtargetFeature<"fp64", "HasFP64", "true",
    "64-bit floating point on register pairs">;
def FeatureSHARCPlus : SubtargetFeature<"sharcplus", "IsSHARCPlus", "true",
    "SHARC+ core", [FeatureVISA, FeatureByteAddr, FeatureFP64]>;

def HasVISA       : Predicate<"Subtarget->hasVISA()">,
                    AssemblerPredicate<(all_of FeatureVISA)>;
def IsSHARCPlus   : Predicate<"Subtarget->isSHARCPlus()">,
                    AssemblerPredicate<(all_of FeatureSHARCPlus)>;
def IsClassic     : Predicate<"!Subtarget->isSHARCPlus()">,
                    AssemblerPredicate<(all_of (not FeatureSHARCPlus))>;
def HasByteAddr   : Predicate<"Subtarget->hasByteAddr()">,
                    AssemblerPredicate<(all_of FeatureByteAddr)>;
def HasFP64       : Predicate<"Subtarget->hasFP64()">,
                    AssemblerPredicate<(all_of FeatureFP64)>;

include "SHARCRegisterInfo.td"
include "SHARCInstrInfo.td"

def : ProcessorModel<"adsp-2136x", NoSchedModel, []>;
def : ProcessorModel<"adsp-2137x", NoSchedModel, []>;
def : ProcessorModel<"adsp-214xx", NoSchedModel, [FeatureVISA]>;
def : ProcessorModel<"adsp-2156x", NoSchedModel, [FeatureSHARCPlus]>;
def : ProcessorModel<"adsp-sc58x", NoSchedModel, [FeatureSHARCPlus]>;
def : ProcessorModel<"adsp-2158x", NoSchedModel, [FeatureSHARCPlus]>;

def SHARCInstrInfo : InstrInfo;
def SHARCAsmParser : AsmParser { let ShouldEmitMatchRegisterName = 1; }
def SHARCAsmParserVariant : AsmParserVariant { let RegisterPrefix = ""; }

def SHARC : Target {
  let InstructionSet = SHARCInstrInfo;
  let AssemblyParsers = [SHARCAsmParser];
  let AssemblyParserVariants = [SHARCAsmParserVariant];
}
```

- [ ] **Step 4: Write `SHARCRegisterInfo.td`.** Encode the Ureg codes from
  PGR p473–474 / PRM p551 as `HWEncoding`. The core of it:

```tablegen
let Namespace = "SHARC" in {
def FName : RegAltNameIndex;
class SHARCReg<bits<7> enc, string n, list<string> alt = []>
    : Register<n, alt> {
  let HWEncoding{6-0} = enc;
  let RegAltNameIndices = !if(!empty(alt), [], [FName]);
}
}

foreach i = 0-15 in {
  def R#i : SHARCReg<!add(0x00, i), "R"#i, ["F"#i]>;
  def I#i : SHARCReg<!add(0x10, i), "I"#i>;
  def M#i : SHARCReg<!add(0x20, i), "M"#i>;
  def L#i : SHARCReg<!add(0x30, i), "L"#i>;
  def B#i : SHARCReg<!add(0x40, i), "B"#i>;
  def S#i : SHARCReg<!add(0x50, i), "S"#i, ["SF"#i]>;
}
```

  Then the group `110` misc registers (FADDR, DADDR, PC, PCSTK, PCSTKP,
  LADDR, CURLCNTR, LCNTR, EMUCLK, EMUCLK2, PX, PX1, PX2, TPERIOD, TCOUNT) and
  group `111` system registers (USTAT1–4, MODE1, MMASK, MODE2, FLAGS, ASTATX,
  ASTATY, STKYX, STKYY, IRPTL, IMASK, IMASKP, LIRPTL). Each low nibble must
  be copied from the PGR Ureg table, not guessed. `MODE1STK` reuses
  `LIRPTL`'s code 0x7D: model it as a second name for the same register
  (`AltNames`), printed when `sharcplus` is set.

  Classes (registers listed in encoding order):

```tablegen
def RFREG  : RegisterClass<"SHARC", [i32, f32], 32, (sequence "R%u", 0, 15)>;
def SREG   : RegisterClass<"SHARC", [i32, f32], 32, (sequence "S%u", 0, 15)>;
def I1REG  : RegisterClass<"SHARC", [i32], 32, (sequence "I%u", 0, 7)>;
def I2REG  : RegisterClass<"SHARC", [i32], 32, (sequence "I%u", 8, 15)>;
def M1REG  : RegisterClass<"SHARC", [i32], 32, (sequence "M%u", 0, 7)>;
def M2REG  : RegisterClass<"SHARC", [i32], 32, (sequence "M%u", 8, 15)>;
def RXMREG : RegisterClass<"SHARC", [i32, f32], 32, (sequence "R%u", 0, 3)>;
def RYMREG : RegisterClass<"SHARC", [i32, f32], 32, (sequence "R%u", 4, 7)>;
def RXAREG : RegisterClass<"SHARC", [i32, f32], 32, (sequence "R%u", 8, 11)>;
def RYAREG : RegisterClass<"SHARC", [i32, f32], 32, (sequence "R%u", 12, 15)>;
def UREG   : RegisterClass<"SHARC", [i32], 32,
                (add RFREG, (sequence "I%u", 0, 15), (sequence "M%u", 0, 15),
                     (sequence "L%u", 0, 15), (sequence "B%u", 0, 15), SREG,
                     MiscRegs, SysRegs)>;
```

  `MiscRegs` and `SysRegs` are `(add …)` lists of the defs above. Define
  `DBLREG` with `RegisterTuples` over even/odd `R` pairs (`F1:0` …
  `F15:14`), with `HWEncoding` set to the even register's number.

- [ ] **Step 5: Write the CMake files and the skeleton C++.** Mirror Xtensa:
  - `CMakeLists.txt`: same tablegen lines as Xtensa, minus `-gen-callingconv`,
    `-gen-dag-isel` and `-gen-sd-node-info`; plus
    `add_subdirectory(MCTargetDesc)` and `add_subdirectory(TargetInfo)`. Add
    `AsmParser`/`Disassembler` subdirectories in later tasks.
  - `SHARCTargetMachine` derives from `CodeGenTargetMachineImpl` with no
    passes. It exists only so `LLVMInitializeSHARCTarget` links, and its
    data layout string is `"e-m:e-p:32:32-i64:32-f64:32-a:0:32-n32"`.
    Copy `SHARCTargetInfo.cpp` from Xtensa, renaming
    (`RegisterTarget<Triple::sharc>(getTheSHARCTarget(), "sharc", "Analog Devices SHARC", "SHARC")`).
  - `SHARCMCAsmInfo` derives from `MCAsmInfoELF`:
    - `CommentString = "//"`, `AllowAdditionalComments = true`
    - `SeparatorString = ";"` is **not** set; `;` is the statement
      terminator, which the AsmParser handles (Task 9)
    - `IsLittleEndian = true`, `CodePointerSize = CalleeSaveStackSlotSize = 4`
    - `MinInstAlignment = 2`, `SupportsDebugInformation = true`
  - `SHARCMCTargetDesc.cpp` registers MCAsmInfo, MCInstrInfo, MCRegInfo and
    MCSubtargetInfo now; later tasks add the rest.

- [ ] **Step 6: Build and run the test.**
  Run: configure as above, then `ninja -C build-sharc llvm-mc && build-sharc/bin/llvm-lit -v llvm/test/MC/SHARC/cpus.s`
  Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add llvm/CMakeLists.txt llvm/lib/Target/SHARC llvm/test/MC/SHARC
git commit -m "[SHARC] Add target skeleton, registers and subtarget features"
```

---

### Task 4: Generator for forms, lengths and vectors (digiemu)

**Files (in `~/GitHub/digiemu/tools/sharcspec/`):**
- Create: `gen_llvm_td.py`, `llvm_vectors.py`
- Create: `~/GitHub/digiemu/tests/test_sharcspec_llvm.py`

**Interfaces:**
- Produces:
  - `gen_llvm_td.py --out DIR` writes `SHARCInstrFormats.td`,
    `SHARCInstrInfo.td`, `SHARCFormLengths.def` and `SHARCWideForms.def`.
    `--diff LLVMDIR` prints a unified diff against the committed files and
    exits 1 if they differ.
  - `llvm_name(form_name) -> str`, e.g. `"Type5a (swap)"` → `"T5a_swap"`,
    `"Type8a_abs"` → `"T8a_abs"`.
  - `llvm_vectors.encode(form_name, **fields) -> int` (the 48-bit window),
    and `llvm_vectors.parcel_bytes(word48, width) -> bytes`.

- [ ] **Step 1: Write the failing tests.** `tests/test_sharcspec_llvm.py`:

```python
import json, subprocess, sys
from pathlib import Path

SPEC = Path(__file__).resolve().parents[1] / "tools" / "sharcspec"
sys.path.insert(0, str(SPEC))

import gen_llvm_td as g
import llvm_vectors as v


def test_llvm_name():
    assert g.llvm_name("Type5a (swap)") == "T5a_swap"
    assert g.llvm_name("Type8a_abs") == "T8a_abs"
    assert g.llvm_name("Type1a") == "T1a"


def test_undocumented_forms_excluded():
    forms = g.load_forms()
    assert not [f for f in forms if "undoc" in f["name"]]
    assert len(forms) == 55   # 61 in decode_table.json minus 6 undocumented


def test_parcel_layout():
    # 48-bit window 0x013E00001012 -> parcels 013E 0000 1012, each LE.
    assert v.parcel_bytes(0x013E00001012, 48) == bytes.fromhex("3e0100001210")
    assert v.parcel_bytes(0xC00100000000, 16) == bytes.fromhex("01c0")
    assert v.parcel_bytes(0x018010120000, 32) == bytes.fromhex("80011210")


def test_vectors_known_forms():
    assert v.encode("Type21a") == 0
    assert v.encode("Type22a", emu=0) == 0x008000000000
    # R0 = R1 + R2 as Type 2a with cond TRUE (11111): compute 0x001012.
    assert v.encode("Type2a", cond=0x1F, compute=0x001012) == 0x013E00001012
    # R0 = 0x12345678 (Type 17a, ureg R0 = 0x00).
    assert v.encode("Type17a", ureg=0x00, data=0x12345678) == 0x0F0012345678
    assert v.encode("Type2b", compute=0x001012) == 0x018010120000


def test_generated_td_has_every_form(tmp_path):
    subprocess.run([sys.executable, str(SPEC / "gen_llvm_td.py"), "--out", str(tmp_path)], check=True)
    td = (tmp_path / "SHARCInstrInfo.td").read_text()
    for f in g.load_forms():
        assert f"def {g.llvm_name(f['name'])} " in td
    assert "undoc" not in td
```

- [ ] **Step 2: Run and watch it fail.**
  Run: `cd ~/GitHub/digiemu && uv run --with pytest python -m pytest tests/test_sharcspec_llvm.py -q`
  Expected: `ModuleNotFoundError: gen_llvm_td`.

- [ ] **Step 3: Write `llvm_vectors.py`.**

```python
"""Encode decode_table.json forms to bytes for LLVM lit-test expectations.

Fields are given by base name (the label without its [hi:lo] slice); a field
split across several slices in the table (e.g. pmi[2:2] + pmi[1:0]) takes one
integer and is scattered by slice.
"""
import json, re
from pathlib import Path

HERE = Path(__file__).resolve().parent
_SLICE = re.compile(r"^(?P<base>[A-Za-z0-9_]+)(?:\[(?P<hi>\d+)(?::(?P<lo>\d+))?\])?$")


def _forms():
    return {f["name"]: f for f in json.loads((HERE / "decode_table.json").read_text())["forms"]}


def encode(form_name, **fields):
    f = _forms()[form_name]
    word = int(f["value"], 16)
    used = set()
    for fl in f["fields"]:
        m = _SLICE.match(fl["label"])
        base = m["base"]
        if base not in fields:
            continue
        used.add(base)
        width = fl["hi"] - fl["lo"] + 1
        # "x[hi:lo]" is a slice of the value; "x[b]" one bit; bare "x" is all of it.
        value_lo = int(m["lo"] or m["hi"] or 0)
        part = (fields[base] >> value_lo) & ((1 << width) - 1)
        word |= part << fl["lo"]
    unknown = set(fields) - used
    if unknown:
        raise KeyError(f"{form_name} has no field(s) {sorted(unknown)}")
    return word


def parcel_bytes(word48, width):
    out = bytearray()
    for i in range(width // 16):
        parcel = (word48 >> (32 - 16 * i)) & 0xFFFF
        out += parcel.to_bytes(2, "little")
    return bytes(out)


def llvm_check(form_name, width, **fields):
    """FileCheck text for -show-encoding, e.g. 'encoding: [0x3e,0x01,...]'."""
    b = parcel_bytes(encode(form_name, **fields), width)
    return "encoding: [" + ",".join(f"0x{x:02x}" for x in b) + "]"
```

  Check `encode` against the table's actual field labels. Type 2a's compute
  is split as `compute[22:16]` + `compute[15:0]`, which the slice logic
  above handles. Type 17a's is `data[31:16]` + `data[15:0]`.

- [ ] **Step 4: Write `gen_llvm_td.py`.** It needs:
  1. **`load_forms()`:** the forms whose name doesn't contain `undoc`.
  2. **`llvm_name()`:** `re.sub(r"^Type", "T", name)`, then `" (x)"` →
     `"_x"`, and any other non-alphanumeric character → `_`.
  3. **Bit positions:** a form of width `W` uses `field bits<W> Inst;`.
     Window bit `b` maps to `Inst{b - (48 - W)}`.
  4. **Fixed bits:** emit `let Inst{k} = v;` for each fixed bit, from
     `mask`/`value`.
  5. **Field operands:** group the slices by base name into one operand.
     Use `LABEL_OPERANDS` for the operand type:

```python
LABEL_OPERANDS = {
    # base label: (operand class, bit width or None to take from slices)
    "compute": ("compute_op", 23),
    "cond": ("cond_op", 5),
    "dmi": ("I1REG", 3), "pmi": ("I2REG", 3),
    "dmm": ("M1REG", 3), "pmm": ("M2REG", 3),
    "dmdreg": ("RFREG", 4), "pmdreg": ("RFREG", 4), "dreg": ("RFREG", 4),
    "ureg": ("UREG", 7),
    "addr": ("addr24_op", 24), "reladdr": ("pcrel_op", None),
    "data": ("imm_op", None),
}
# Everything else (single-bit flags such as dmd, pmd, b, a, j, ci, lr, e, x,
# emu, mode, and small selector fields) becomes u<N>imm with N = total width.
```

     A register operand is encoded with `getRegEncoding` and the class's
     index base (I1REG: `enc - 0x10`; I2REG: `enc - 0x18`; M1REG:
     `enc - 0x20`; M2REG: `enc - 0x28`; RFREG: `enc`; UREG: the 7-bit
     `HWEncoding`). `gen_llvm_td.py` emits `EncoderMethod`/`DecoderMethod`
     names (`encodeI1Reg`/`DecodeI1REGRegisterClass`, …) that Task 7
     implements.
  6. **Unused bits:** emit `let Inst{k} = 0;` plus `let SoftFail{k} = 1;`,
     so the decoder accepts nonzero values with a soft-fail.
  7. **One def per form:**
     `def T2a : SHARCInst48<(outs), (ins cond_op:$cond, compute_op:$compute), "", []>`.
     The AsmString is empty because printing is slot-driven (Task 7). Set
     `DecoderNamespace` to `"ISA"` for `isa` forms of width 48 and `"VISA"`
     for every `visa` form. A 48-bit form that is both is emitted **twice**
     (`T2a` and `T2a_V` with `isCodeGenOnly = 1`, `DecoderNamespace="VISA"`),
     because TableGen gives each def one namespace. The predicate is
     `IsSHARCPlus` for forms whose `source` notes say SHARC+ only (3d, 4d,
     7d, 14d, 26a) and `HasVISA` for 16/32-bit forms.
  8. **`SHARCInstrFormats.td`:**

```tablegen
class SHARCInst<int width, dag outs, dag ins, string asm, list<dag> pat>
    : Instruction {
  let Namespace = "SHARC";
  let Size = !div(width, 8);
  let OutOperandList = outs;
  let InOperandList = ins;
  let AsmString = asm;
  let Pattern = pat;
  let hasSideEffects = 1;
}
class SHARCInst48<dag o, dag i, string a, list<dag> p> : SHARCInst<48, o, i, a, p> {
  field bits<48> Inst; field bits<48> SoftFail = 0;
}
class SHARCInst32<dag o, dag i, string a, list<dag> p> : SHARCInst<32, o, i, a, p> {
  field bits<32> Inst; field bits<32> SoftFail = 0;
}
class SHARCInst16<dag o, dag i, string a, list<dag> p> : SHARCInst<16, o, i, a, p> {
  field bits<16> Inst; field bits<16> SoftFail = 0;
}
```

  9. **`SHARCFormLengths.def`:** one line per VISA form,
     `SHARC_FORM(T2c, 16, 0xf00000000000ULL, 0xc00000000000ULL, /*lead=*/4, /*fixed=*/4)`,
     where `lead` and `fixed` are computed exactly as in `sharc_decode.py`'s
     `Decoder.__init__`, so both use the same length rule.
  10. **`SHARCWideForms.def`:** `SHARC_WIDE(T2c, T2a)` for each narrow form.
      Pair by type number (strip the `b`/`c` suffix and add `a`); e.g.
      `T17b`→`T17a`, `T9b_rel`→`T9a_rel`, `T25c_rframe`→`T25a_rframe`,
      `T5b_move`→`T5a_move`. Fail loudly if a narrow form has no partner.
  11. **Headers:** every emitted file starts with
      `// Generated by digiemu tools/sharcspec/gen_llvm_td.py from public ADI manuals on <date>. Maintained by hand after import; regenerate with --diff to review table changes.`
  12. **`--diff LLVMDIR`:** generate into a temporary directory and run
      `difflib.unified_diff` against `LLVMDIR/<file>` for each file.

- [ ] **Step 5: Run the tests.**
  Run: `uv run --with pytest python -m pytest tests/test_sharcspec_llvm.py -q`
  Expected: PASS. If `len(forms) == 55` fails, print the names and fix the
  filter or the count, using `decode_table.json` as the truth.

- [ ] **Step 6: Stage the change in digiemu** (the user commits; see Global
  Constraints).

```bash
cd ~/GitHub/digiemu && git add tools/sharcspec/gen_llvm_td.py tools/sharcspec/llvm_vectors.py tests/test_sharcspec_llvm.py
```

---

### Task 5: Generator for the compute tables (digiemu)

**Files:**
- Modify: `~/GitHub/digiemu/tools/sharcspec/gen_llvm_td.py` (add `emit_compute`)
- Modify: `~/GitHub/digiemu/tests/test_sharcspec_llvm.py`

**Interfaces:**
- Produces:
  - `SHARCInstrCompute.td`, with defs named `C_<table>_<opcode-bits>[_<mod>]`,
    e.g. `C_ALU_00000001`, `C_MUL_0000F00x_SSI` (an `x` in the pattern is
    expanded), and `SC_0000` for short compute. Namespace `Compute`
    (23-bit `Inst`) or `ShortCompute` (12-bit `Inst`).
  - `SHARCComputeSyntax.def`, with lines
    `SHARC_COMPUTE(C_ALU_00000001, "$rn = $rx + $ry")`.
  - `SHARCShortToCompute.def`, with lines `SHARC_SHORT(SC_0000, C_ALU_00000001)`
    (the matching syntax, with `rn` repeated as `rx`).

- [ ] **Step 1: Write the failing tests.** Append:

```python
def test_compute_counts(tmp_path):
    subprocess.run([sys.executable, str(SPEC / "gen_llvm_td.py"), "--out", str(tmp_path)], check=True)
    td = (tmp_path / "SHARCInstrCompute.td").read_text()
    syn = (tmp_path / "SHARCComputeSyntax.def").read_text()
    c = json.loads((SPEC / "compute_table.json").read_text())
    # Every ALU row appears exactly once (no mod expansion in ALU rows).
    assert td.count("def C_ALU_") == len(c["aluop_32_40bit"]["rows"])
    assert "def C_ALU_00000001 " in td
    assert 'SHARC_COMPUTE(C_ALU_00000001, "$rn = $rx + $ry")' in syn
    # Short compute rows.
    assert td.count("def SC_") == len(c["shortcompute"]["rows"])


def test_compute_encoding_r0_r1_r2():
    # R0 = R1 + R2: mf=0 cu=00 opcode=00000001 rn=0 rx=1 ry=2 -> 0x001012
    assert g.compute_word("C_ALU_00000001", rn=0, rx=1, ry=2) == 0x001012


def test_every_row_emitted_or_listed():
    rows, skipped = g.compute_rows()
    # Nothing silently dropped: every row is emitted or explicitly skipped with a reason.
    c = json.loads((SPEC / "compute_table.json").read_text())
    total = sum(len(v["rows"]) for k, v in c.items() if isinstance(v, dict) and "rows" in v)
    assert len(rows) + len(skipped) >= total
    assert all(reason for _, reason in skipped)
```

- [ ] **Step 2: Run and watch it fail.** Expected: `AttributeError: compute_word`.

- [ ] **Step 3: Implement `compute_rows()`, `compute_word()` and
  `emit_compute()`.** Rules:
  1. **Unit bits:** a single-function row is `mf=0` with `cu` set by the
     table: ALU `00`, MUL `01`, SHIFT `10`. The opcode is the row's 8-bit
     pattern at bits 19–12. Registers go at `rn[11:8]`, `rx[7:4]`,
     `ry[3:0]` (`Figure18-1_Compute`).
  2. **Pattern characters:** spaces are removed. `0`/`1` are fixed. `x` is
     expanded over both values unless the syntax names it. Letters that
     match a MOD table (`mod1_table`, `mod2_table`, `mod3_table`; `F` in
     MULOP is the fractional/integer selector the row's `MOD` refers to) are
     expanded per MOD row, with the option name appended to the def name and
     to the syntax as `(SSI)` etc.
  3. **Syntax templating:** in the syntax string, `RN/RX/RY/FN/FX/FY` →
     `$rn/$rx/$ry` with operand class `RFREG` for R and `FREG_op` (same
     registers, F printing) for F. Multifunction rows use the
     `Table18-15..19` layouts: `$rm`, `$ra`, `$rs` are RFREG (4-bit), and
     `$rxm`/`$rym`/`$rxa`/`$rya` are `RXMREG`/`RYMREG`/`RXAREG`/`RYAREG`
     (2-bit). The `R3-0`-style placeholders in the multifunction syntax
     stand for those 2-bit operands. 64-bit rows use `DBLREG` with the
     `Table18-23`/`18-25` layouts.
  4. **Shift-immediate:** emit both the register form (`$ry` at 3–0) and the
     immediate form (the `Figure18-3_ShiftImm` layout, `data[15:8]` plus
     `dataex[26:23]`). The immediate form is a **Type 6a-only** compute and
     lives in its own namespace, `ShiftImm`.
  5. **Skipping:** any row the rules can't express goes into `skipped` with
     a reason string, and the generator prints a summary. Skipped rows must
     be handled by hand in `SHARCInstrCompute.td` during Task 6. They are
     never dropped silently.
  6. **`compute_word(name, **regs)`:** returns the 23-bit field for tests and
     `llvm_vectors`.

- [ ] **Step 4: Run the tests.** Expected: PASS. Review the skipped-row
  summary and paste it into the Task 6 notes.

- [ ] **Step 5: Stage the change.**
  `cd ~/GitHub/digiemu && git add tools/sharcspec/gen_llvm_td.py tests/test_sharcspec_llvm.py`

---

### Task 6: Import the generated TableGen and the slot table

**Files:**
- Create (generated by `gen_llvm_td.py --out llvm/lib/Target/SHARC`):
  `SHARCInstrFormats.td`, `SHARCInstrInfo.td` (replacing the placeholder),
  `SHARCInstrCompute.td`, `SHARCFormLengths.def`, `SHARCWideForms.def`,
  `SHARCComputeSyntax.def`, `SHARCShortToCompute.def`
- Create: `llvm/lib/Target/SHARC/SHARCOperands.td`, `SHARCFormSlots.def`
- Modify: `SHARC.td` (include the operands and compute files)
- Test: `llvm/test/MC/SHARC/tblgen-forms.test`

**Interfaces:**
- Produces: `SHARC::T2a`, `SHARC::T21c`, … and `SHARC::C_ALU_00000001`, …
  as `MCInst` opcodes. `SHARCFormSlots.def` rows:
  `SHARC_SLOTS(T1a, SLOT_COMPUTE | SLOT_DM | SLOT_PM, /*cond*/-1, /*compute*/8, /*dm*/{0,1,2,4}, /*pm*/{3,5,6,7})`,
  i.e. which slots a form carries and the `MCInst` operand indices for each
  slot's fields. Slot enum: `SLOT_COND`, `SLOT_COMPUTE`, `SLOT_ELSE_COMPUTE`,
  `SLOT_DM`, `SLOT_PM`, `SLOT_UREG_MOVE`, `SLOT_IMM_LOAD`, `SLOT_BRANCH`,
  `SLOT_RETURN`, `SLOT_LOOP`, `SLOT_MODIFY`, `SLOT_BITOP`, `SLOT_STACK`,
  `SLOT_NOP`, `SLOT_IDLE`, `SLOT_SYNC`, `SLOT_SHIFTIMM`, `SLOT_SWAP`.

- [ ] **Step 1: Write the failing test.** `llvm/test/MC/SHARC/tblgen-forms.test`:

```
# RUN: llvm-tblgen -I %p/../../../include -I %p/../../../lib/Target/SHARC \
# RUN:   %p/../../../lib/Target/SHARC/SHARC.td --dump-json -o %t.json
# RUN: %python -c "import json,sys; d=json.load(open(sys.argv[1])); \
# RUN:   import re; n=[k for k in d['!instanceof']['Instruction'] if re.match(r'T[0-9]', k)]; \
# RUN:   print(len(set(x.removesuffix('_V') for x in n)))" %t.json | FileCheck %s
# CHECK: 55
```

- [ ] **Step 2: Run and watch it fail.** Expected: `0` printed, since only the
  placeholder exists.

- [ ] **Step 3: Generate and include.**
  `uv run --project ~/GitHub/digiemu python ~/GitHub/digiemu/tools/sharcspec/gen_llvm_td.py --out llvm/lib/Target/SHARC`.
  Write `SHARCOperands.td` with the operand classes the generator
  references:

```tablegen
def compute_op : Operand<i32> {
  let PrintMethod = "printComputeOperand";
  let EncoderMethod = "getComputeOpValue";
  let DecoderMethod = "decodeComputeOperand";
  let OperandType = "OPERAND_IMMEDIATE";
}
def cond_op : Operand<i32> {
  let PrintMethod = "printCondOperand";
  let DecoderMethod = "decodeUImm<5>";
}
def addr24_op : Operand<i32> {
  let EncoderMethod = "getAddr24OpValue";
  let DecoderMethod = "decodeUImm<24>";
}
def pcrel_op : Operand<i32> {
  let EncoderMethod = "getPCRelOpValue";
  let DecoderMethod = "decodePCRel";
  let OperandType = "OPERAND_PCREL";
}
def imm_op : Operand<i32> {
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImm<32>";
}
foreach n = [1, 2, 3, 4, 5, 6, 7, 8, 16] in
  def u#n#imm : Operand<i32> { let DecoderMethod = "decodeUImm<"#n#">"; }
def FREG_op : RegisterOperand<RFREG> { let PrintMethod = "printFReg"; }
```

  If the build reports TableGen decoder conflicts within one namespace and
  width, that is a real ambiguity in the table. Record it, and resolve it
  with `DecoderNamespace` splits or predicates, citing manual pages in the
  `.td` comment. Then hand-write the compute rows skipped in Task 5.

- [ ] **Step 4: Write `SHARCFormSlots.def`.** Write one row per form, by hand,
  from the PGR "Instruction Set Types" chapter (p372–374 and the per-type
  pages). Each row cites its page in a trailing comment. This table is the
  manual's syntax, written as data. It's the main piece of hand-reviewed
  knowledge in M1, and Tasks 7, 9 and 13 depend on it.

- [ ] **Step 5: Build and run the test.**
  Run: `ninja -C build-sharc llvm-mc llvm-tblgen && build-sharc/bin/llvm-lit -v llvm/test/MC/SHARC/tblgen-forms.test`
  Expected: PASS (55).

- [ ] **Step 6: Commit**

```bash
git add llvm/lib/Target/SHARC llvm/test/MC/SHARC/tblgen-forms.test
git commit -m "[SHARC] Import generated instruction forms, compute table and slot table"
```

---

### Task 7: ISA disassembler and printer

**Files:**
- Create: `Disassembler/{CMakeLists.txt,SHARCDisassembler.cpp}`, `MCTargetDesc/SHARCInstPrinter.{h,cpp}`, `MCTargetDesc/SHARCParcels.h`
- Modify: `SHARCMCTargetDesc.cpp` (register the printer), `SHARC/CMakeLists.txt` (`add_subdirectory(Disassembler)`)
- Test: `llvm/test/MC/Disassembler/SHARC/{lit.local.cfg,isa-basic.txt,isa-forms.txt}`

**Interfaces:**
- Consumes: the generated `DecoderTableISA48`, `DecoderTableCompute23` and
  `SHARCFormSlots.def`.
- Produces:
  - `SHARCParcels.h`: `uint64_t readWindow(ArrayRef<uint8_t>, unsigned &Avail)`
    (big-parcel-first, LE parcels, zero-padded to 3 parcels, `Avail` = real
    parcels) and `void writeParcels(uint64_t Window48, unsigned Width, SmallVectorImpl<char>&)`.
  - Printer entry point `printInst`, with a print routine per slot.

- [ ] **Step 1: Write the failing tests.** Expected bytes come from
  `llvm_vectors.py`; spot-check each one against the PGR page cited in the
  comment. `isa-basic.txt`:

```
# RUN: llvm-mc --disassemble -triple=sharc -mcpu=adsp-2137x %s | FileCheck %s

# Type 21a, PGR ch.9 NOP
# CHECK: NOP;
0x00 0x00 0x00 0x00 0x00 0x00

# Type 22a IDLE
# CHECK: IDLE;
0x80 0x00 0x00 0x00 0x00 0x00

# Type 2a, cond TRUE, compute ALU 00000001 rn=0 rx=1 ry=2
# CHECK: R0 = R1 + R2;
0x3e 0x01 0x00 0x00 0x12 0x10

# Type 2a, cond EQ (00000)
# CHECK: IF EQ R0 = R1 + R2;
0x00 0x01 0x00 0x00 0x12 0x10

# Type 17a, ureg R0, data 0x12345678
# CHECK: R0 = 0x12345678;
0x00 0x0f 0x34 0x12 0x78 0x56

# Type 17a, ureg I0 (0x10)
# CHECK: I0 = 0x12345678;
0x10 0x0f 0x34 0x12 0x78 0x56
```

  `isa-forms.txt`: one vector per 48-bit form (generate the lines with a
  small loop over `llvm_vectors.encode` using representative field values),
  and a CHECK line written from the manual's syntax for that form.
  `lit.local.cfg` is the same as Task 3's.

- [ ] **Step 2: Run and watch it fail.**
  Expected: `invalid instruction encoding` warnings or an unknown-target
  error.

- [ ] **Step 3: Implement `SHARCParcels.h`.**

```cpp
#ifndef LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCPARCELS_H
#define LLVM_LIB_TARGET_SHARC_MCTARGETDESC_SHARCPARCELS_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>

namespace llvm::SHARC {

/// Instructions are stored as 16-bit little-endian parcels, most significant
/// parcel first. Returns the 48-bit window with missing parcels as zero.
inline uint64_t readWindow(ArrayRef<uint8_t> Bytes, unsigned &Avail) {
  uint64_t W = 0;
  Avail = 0;
  for (unsigned I = 0; I < 3; ++I) {
    uint64_t P = 0;
    if (Bytes.size() >= 2 * I + 2) {
      P = Bytes[2 * I] | (uint64_t(Bytes[2 * I + 1]) << 8);
      ++Avail;
    }
    W |= P << (32 - 16 * I);
  }
  return W;
}

inline void writeParcels(uint64_t Window48, unsigned Width,
                         SmallVectorImpl<char> &CB) {
  for (unsigned I = 0; I < Width / 16; ++I) {
    uint16_t P = (Window48 >> (32 - 16 * I)) & 0xFFFF;
    CB.push_back(char(P & 0xFF));
    CB.push_back(char(P >> 8));
  }
}

} // namespace llvm::SHARC
#endif
```

- [ ] **Step 4: Implement the disassembler.** Mirror
  `XtensaDisassembler.cpp`'s class shape. Its `getInstruction`, for now
  (Task 8 adds VISA):

```cpp
DecodeStatus SHARCDisassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                               ArrayRef<uint8_t> Bytes,
                                               uint64_t Address,
                                               raw_ostream &CS) const {
  unsigned Avail;
  uint64_t W = SHARC::readWindow(Bytes, Avail);
  if (Avail < 3) {
    Size = Avail * 2;
    return MCDisassembler::Fail;
  }
  Size = 6;
  return decodeInstruction(DecoderTableISA48, MI, W, Address, this, STI);
}
```

  Operand decoders:
  - `decodeComputeOperand(MCInst &MI, uint64_t Field, uint64_t Addr, const MCDisassembler *D)`
    builds a sub-`MCInst`, runs `decodeInstruction(DecoderTableCompute23, Sub, Field, …)`,
    and appends `MCOperand::createInst(Ctx.createMCInst(Sub))` (allocate it
    through `D->getContext()`).
  - `decodeUImm<N>`
  - register-class decoders (`DecodeRFREGRegisterClass`,
    `DecodeI1REGRegisterClass` = `SHARC::I0 + field`, …, and
    `DecodeUREGRegisterClass`, which looks up the 7-bit code in a table
    built from `SHARCRegisterInfo.td` order)
  - `decodePCRel`: sign-extend the field and store it as an immediate in
    instruction units

- [ ] **Step 5: Implement the printer.** `printInst` looks up the opcode in
  `SHARCFormSlots.def` and prints the slots in manual order:
  `[IF cond] compute|branch…, DM move, PM move [modifiers] ;`.
  - `printCondOperand` prints nothing for TRUE (11111) and `IF <name> `
    otherwise. Take the condition names from PGR p475 (EQ, LT, LE, AC, AV,
    MV, MS, SV, SZ, FLAG0_IN…FLAG3_IN, TF, BM, LCE, plus the `NOT` forms
    with bit 4 set: NE, GE, GT, NOT AC, …).
  - `printComputeOperand` prints the nested `MCInst` using the syntax string
    from `SHARCComputeSyntax.def`, substituting operands in order. FREG
    operands print through the `FName` alt name.
  - Printing is uppercase. Instructions end with `;`.

- [ ] **Step 6: Run the tests.**
  Run: `ninja -C build-sharc llvm-mc && build-sharc/bin/llvm-lit -v llvm/test/MC/Disassembler/SHARC`
  Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add llvm/lib/Target/SHARC llvm/test/MC/Disassembler/SHARC
git commit -m "[SHARC] Add ISA disassembler and slot-driven instruction printer"
```

---

### Task 8: VISA length classifier and 16/32-bit decoding

**Files:**
- Modify: `Disassembler/SHARCDisassembler.cpp`
- Test: `llvm/test/MC/Disassembler/SHARC/visa-basic.txt`, `visa-forms.txt`, `isa-visa-collision.txt`

**Interfaces:**
- Consumes: `SHARCFormLengths.def`, `DecoderTableVISA16/32/48`.
- Produces: `static unsigned visaLength(uint64_t Window)`, which returns
  16, 32, 48 or 0 (no match).

- [ ] **Step 1: Write the failing tests.** `visa-basic.txt`:

```
# RUN: llvm-mc --disassemble -triple=sharc -mcpu=adsp-214xx -mattr=+visa %s | FileCheck %s

# Type 21c 16-bit NOP (parcel 0x0001)
# CHECK: NOP;
0x01 0x00
# Type 2c short compute RN = RN + RX, rn=0 rx=1 (parcel 0xC001)
# CHECK: R0 = R0 + R1;
0x01 0xc0
# Type 2b compute R0 = R1 + R2 (window 0x018010120000)
# CHECK: R0 = R1 + R2;
0x80 0x01 0x12 0x10
# Type 2a in VISA still decodes (48-bit)
# CHECK: IF EQ R0 = R1 + R2;
0x00 0x01 0x00 0x00 0x12 0x10
```

  `isa-visa-collision.txt` decodes the same bytes, a Type 2c parcel `0xC001`
  followed by 4 zero bytes, twice. The first RUN uses `-mcpu=adsp-2137x`
  (ISA) and expects the Type 10a reading. The second uses `+visa` and
  expects `R0 = R0 + R1;` then `NOP;`. Write the Type 10a expected text
  from the PGR Type 10 page.

- [ ] **Step 2: Run and watch it fail.** Expected: the 16/32-bit lines fail
  to decode.

- [ ] **Step 3: Implement the classifier and dispatch.**

```cpp
namespace {
struct FormLen {
  uint64_t Mask, Value;
  uint8_t Width, Lead, Fixed;
};
const FormLen VisaForms[] = {
#define SHARC_FORM(NAME, W, MASK, VAL, LEAD, FIXED) {MASK, VAL, W, LEAD, FIXED},
#include "SHARCFormLengths.def"
#undef SHARC_FORM
};
} // namespace

// Same rule as digiemu's reference decoder: the matching form with the
// longest run of fixed bits from bit 47 wins, then the most fixed bits.
static unsigned visaLength(uint64_t W) {
  const FormLen *Best = nullptr;
  for (const FormLen &F : VisaForms) {
    if ((W & F.Mask) != F.Value)
      continue;
    if (!Best || F.Lead > Best->Lead ||
        (F.Lead == Best->Lead && F.Fixed > Best->Fixed))
      Best = &F;
  }
  return Best ? Best->Width : 0;
}
```

  In `getInstruction`, when `STI.hasFeature(SHARC::FeatureVISA)` and the
  section is VISA (for now just the feature; Task 12 adds the section
  flag):
  1. Compute `Width = visaLength(W)`. If it is 0, or if
     `Width / 16 > Avail`, return Fail with `Size = 2`.
  2. Shift: a 32-bit form is decoded from `W >> 16`, a 16-bit one from
     `W >> 32`.
  3. Call `decodeInstruction(DecoderTableVISA##Width, …)`.
  4. Set `Size = Width / 8`.

  If two forms tie in the classifier with equal width, they don't affect
  the length; TableGen's table decides between them.

- [ ] **Step 4: Run the tests.** Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add llvm/lib/Target/SHARC/Disassembler llvm/test/MC/Disassembler/SHARC
git commit -m "[SHARC] Decode VISA 16/32/48-bit instructions"
```

---

### Task 9: AsmParser and code emitter, part 1 (compute, NOP, IDLE, Ureg = imm)

**Files:**
- Create: `AsmParser/{CMakeLists.txt,SHARCAsmParser.cpp,SHARCComputeParser.h,SHARCComputeParser.cpp}`, `MCTargetDesc/SHARCMCCodeEmitter.cpp`, `MCTargetDesc/SHARCAsmBackend.cpp` (minimal: no fixups yet), `MCTargetDesc/SHARCELFObjectWriter.cpp` (returns `R_SHARC_NONE` for now)
- Modify: `SHARCMCTargetDesc.cpp` (register the emitter, backend and parser), `SHARC/CMakeLists.txt`
- Test: `llvm/test/MC/SHARC/{basic.s,compute.s,invalid-basic.s,roundtrip-isa.s}`

**Interfaces:**
- Consumes: `SHARCFormSlots.def`, `SHARCComputeSyntax.def`, `SHARCParcels.h`.
- Produces:
  - `struct SlotSet { unsigned Mask; std::optional<unsigned> Cond; std::optional<MCInst> Compute; /* DM, PM, … filled in Task 10 */ };`
  - `bool parseComputeSlot(MCAsmParser&, MCInst &Out)` in `SHARCComputeParser.h`
  - `unsigned selectForm(const SlotSet&, const MCSubtargetInfo&, bool VISASection)`,
    which returns an opcode or 0 and is used again in Task 13
  - `uint64_t SHARCMCCodeEmitter::getComputeOpValue(const MCInst&, unsigned OpNo, SmallVectorImpl<MCFixup>&, const MCSubtargetInfo&) const`

- [ ] **Step 1: Write the failing tests.** `basic.s` (expectations from
  `llvm_vectors.llvm_check`):

```asm
# RUN: llvm-mc -triple=sharc -mcpu=adsp-2137x -show-encoding %s | FileCheck %s

nop;
# CHECK: NOP;   # encoding: [0x00,0x00,0x00,0x00,0x00,0x00]
idle;
# CHECK: IDLE;  # encoding: [0x80,0x00,0x00,0x00,0x00,0x00]
r0 = r1 + r2;
# CHECK: R0 = R1 + R2;  # encoding: [0x3e,0x01,0x00,0x00,0x12,0x10]
IF EQ R0 = R1 + R2;
# CHECK: IF EQ R0 = R1 + R2;  # encoding: [0x00,0x01,0x00,0x00,0x12,0x10]
R0 = 0x12345678;
# CHECK: R0 = 0x12345678;  # encoding: [0x00,0x0f,0x34,0x12,0x78,0x56]
i0 = 0x12345678;
# CHECK: I0 = 0x12345678;  # encoding: [0x10,0x0f,0x34,0x12,0x78,0x56]
```

  `compute.s`: one line per generated compute opcode, from an
  `llvm_vectors` loop over `SHARCComputeSyntax.def` with registers rn=3,
  rx=5, ry=9, each encoded as Type 2a TRUE.

  `invalid-basic.s`:

```asm
# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2137x %s 2>&1 | FileCheck %s
r0 = r1 +;
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error: expected register
r0 = r1 + r2
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error: expected ';'
r0 = q1 + r2;
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error: unknown register 'q1'
```

  `roundtrip-isa.s`: assemble `basic.s` to an object, disassemble it with
  `llvm-objdump -d`, and check the same text.

- [ ] **Step 2: Run and watch it fail.**
  Expected: `error: unable to create target`, or no AsmParser registered.

- [ ] **Step 3: Implement the code emitter.**

```cpp
void SHARCMCCodeEmitter::encodeInstruction(const MCInst &MI,
                                           SmallVectorImpl<char> &CB,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  unsigned Width = MCII.get(MI.getOpcode()).getSize() * 8;
  uint64_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
  SHARC::writeParcels(Bits << (48 - Width), Width, CB);
}

uint64_t SHARCMCCodeEmitter::getComputeOpValue(
    const MCInst &MI, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  const MCInst *Sub = MI.getOperand(OpNo).getInst();
  return getBinaryCodeForInstr(*Sub, Fixups, STI) & 0x7FFFFF;
}
```

  Mirror Xtensa's `createXtensaMCCodeEmitter` for the factory. The register
  encoders (`encodeI1Reg`, …) subtract the class base from
  `Ctx.getRegisterInfo()->getEncodingValue(Reg)`.

- [ ] **Step 4: Implement the parser core.**
  - `ParseInstruction`, or the current equivalent name in
    `MCTargetAsmParser` (copy it from `XtensaAsmParser.cpp`), reads tokens
    up to `;`.
  - **The first token isn't a mnemonic.** Use the
    `parseInstruction(ParseInstructionInfo&, StringRef Name, SMLoc, OperandVector&)`
    hook to receive it, then hand-parse the rest. Return the built `MCInst`
    through a single custom `SHARCOperand` of kind `Inst`, and override
    `matchAndEmitInstruction` to emit it directly. This is how Hexagon
    bypasses the matcher.
  - **Slot splitter:** split at top-level commas (parenthesis depth 0). Take
    a leading `IF <cond>` into `SlotSet::Cond`. A slot is a compute if it
    matches `parseComputeSlot`. `UREG = <expr>` with an absolute expression
    becomes `SLOT_IMM_LOAD`. `NOP`/`IDLE`/`EMUIDLE`/`SYNC` are keywords.
  - **`parseComputeSlot`:** a pattern matcher built once from
    `SHARCComputeSyntax.def`. Each pattern is tokenized into literal tokens
    and `$operand` placeholders. A placeholder accepts a register of the
    right class (an R name for RFREG, an F name for FREG_op; ranges for the
    2-bit multifunction classes). Literal keywords compare
    case-insensitively. When several patterns match, take the first in
    `.def` order; the generator orders rows so that more specific patterns
    come first.
  - **`selectForm`:** go through the forms in `SHARCFormSlots.def` order,
    smallest width first in VISA sections, and pick the first form whose
    slot mask equals the parsed mask and whose operands fit: the register
    class, and immediate ranges via `isUInt<N>`/`isInt<N>`. Before enabling
    Type 17b (16-bit data), the executor must read the PGR Type 17 page to
    learn whether `data16` is sign- or zero-extended. Encode that rule in
    `fitsImm16ForType17b()`, with the page cited, and add a test for a
    negative value.

- [ ] **Step 5: Run the tests.**
  Run: `ninja -C build-sharc llvm-mc llvm-objdump && build-sharc/bin/llvm-lit -v llvm/test/MC/SHARC`
  Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add llvm/lib/Target/SHARC llvm/test/MC/SHARC
git commit -m "[SHARC] Add algebraic AsmParser core and code emitter"
```

---

### Task 10: AsmParser part 2: memory moves, multifunction, branches and the remaining slots

**Files:**
- Modify: `AsmParser/SHARCAsmParser.cpp`, `SHARCFormSlots.def` (if gaps turn up)
- Test: `llvm/test/MC/SHARC/{moves.s,multifunction.s,branches.s,loops-syntax.s,sysreg.s,sharcplus.s,visa-select.s,invalid-forms.s}`

**Interfaces:**
- Consumes: Task 9's `SlotSet`/`selectForm`.
- Produces: `SlotSet` gains `DM`, `PM`, `UregMove`, `Branch`, `Return`,
  `Loop`, `Modify`, `BitOp`, `Stack`, `Shift`, `Swap`, `ElseCompute` and
  `Modifiers` (a bitmask: `MOD_DB`, `MOD_LA`, `MOD_CI`, `MOD_LR`, `MOD_LW`,
  `MOD_BW`, `MOD_BWSE`, `MOD_SW`, `MOD_SWSE`, `MOD_EX`, `MOD_NW`, `MOD_F`).

Work through the slot kinds one at a time. Each has the same TDD cycle: add
test lines, watch them fail, implement, watch them pass. The order:

- [ ] **Step 1: DM/PM moves (Types 1a/1b, 3a/3b/3c, 4a/4b).**
  - Test lines, with expectations from `llvm_vectors`, the PGR p376
    examples, and their CHECK text:
    `R7=BSET R6 BY R0, DM(I0,M3)=R5, PM(I11,M15)=R4;` and
    `R8=DM(I4,M1), PM(I12,M12)=R0;`
  - Parse `DM(Ia, Mb)` / `PM(Ic, Md)` in both directions. A post-modify
    `DM(Ia,Mb)` and a pre-modify `DM(Mb,Ia)` select different `u` bits;
    take that from the form pages.
  - `(LW)` sets `MOD_LW`.
  - Implement, then run `moves.s`. Expected: PASS.

- [ ] **Step 2: Multifunction.** Test the PRM p529 example
  `F8=F3*F7, F12=F11+F15, F13=F11-F15;`, written with registers that fit the
  restricted classes. The compute parser must accept the whole
  comma-separated multifunction group as **one** compute slot: try the
  multifunction patterns on the joined slots before splitting. Run
  `multifunction.s`. Expected: PASS.

- [ ] **Step 3: Branches and returns (8a, 9a/9b, 10a, 11a/11c, 25a/25c).**
  Test lines (from PGR p402 and p407–410):
  - `IF AV JUMP(PC,0x00A4)(LA);`
  - `JUMP (PC,2)(DB,CI);`
  - `IF EQ CALL(PC,17)(DB), ELSE R6=R6-1;`
  - `IF NE JUMP(PC,0x20), ELSE F12=FLOAT R10 BY R3, R6=DM(I5,M0);`
  - `RTS(DB);`
  - `CJUMP lbl (DB);`
  - `RFRAME;`

  Absolute label targets are handled in Task 11; here use only numeric
  `(PC, n)` and absolute numbers. Run `branches.s`. Expected: PASS.

- [ ] **Step 4: Loops, modify, bit ops, stack, shift-immediate, swap.**
  Cover Types 12a, 13a, 7a/7b, 18a, 19a, 20a, 6a and 5a/5b:
  - `LCNTR=10, DO 5 UNTIL LCE;` (numeric loop end for now)
  - `MODIFY(I1,M2);`
  - `I3=MODIFY(I1,M2);`
  - `BIT SET MODE1 0x1000;`
  - `PUSH LOOP, PUSH STS;`
  - `R1=LSHIFT R2 BY 4;`
  - `R0<->S0;`

  Run `loops-syntax.s` and `sysreg.s`. Expected: PASS.

- [ ] **Step 5: SHARC+ forms (3d, 4d, 7d, 14d, 26a).** These are byte modes,
  exclusive access, `B2W`/`W2B` and `SYNC`, plus the Type 12a `(F)` bit.
  `sharcplus.s` runs once with `-mcpu=adsp-2156x` (PASS) and once with
  `-mcpu=adsp-214xx`, where it must fail with
  `error: instruction requires: sharcplus`. Take the expected encodings from
  `llvm_vectors` and spot-check them against the PRM figures (p321, p326,
  p337, p355, p388), which the ISA research flagged as hard to read.

- [ ] **Step 6: VISA selection.** `visa-select.s` with `-mcpu=adsp-214xx`,
  inside a `.visa` section (the directive comes in Task 12; until then
  select by `-mattr=+visa` and a parser option `-sharc-default-visa`):
  - `r0 = r0 + r1;` selects 2c (2 bytes)
  - `r0 = r1 + r2;` selects 2b (4 bytes)
  - `IF EQ r0 = r1 + r2;` selects 2a (6 bytes)
  - `nop;` selects 21c

  Check the encodings.

- [ ] **Step 7: Illegal combinations.** `invalid-forms.s`: three memory
  moves; a compute and a branch without `ELSE` where no form exists; Type
  10a in VISA. Each must produce an error naming the slots, e.g.
  `error: no SHARC instruction form combines compute, DM move and PM move with an immediate`.

- [ ] **Step 8: Round trip.** Extend `roundtrip-isa.s` and add
  `roundtrip-visa.s`, assembling every test above, disassembling, and
  checking identical text. Run the whole SHARC lit directory. Expected:
  PASS.

- [ ] **Step 9: Commit**

```bash
git add llvm/lib/Target/SHARC llvm/test/MC/SHARC
git commit -m "[SHARC] Parse memory moves, multifunction, branches, loops and SHARC+ forms"
```

---

### Task 11: Fixups and relocations (needs Task 1)

**Files:**
- Create: `MCTargetDesc/SHARCFixupKinds.h`
- Modify: `SHARCMCCodeEmitter.cpp` (`getPCRelOpValue`, `getAddr24OpValue`, `getImmOpValue` emit fixups), `SHARCAsmBackend.cpp`, `SHARCELFObjectWriter.cpp`
- Modify: `llvm/docs/SHARCUsage.rst` (fill in "Relocations")
- Test: `llvm/test/MC/SHARC/{reloc-isa.s,reloc-visa.s,fixup-local.s,fixup-range.s,data.s}`

**Interfaces:**
- Produces: fixup kinds `fixup_sharc_pcrel24`, `fixup_sharc_pcrel23`,
  `fixup_sharc_pcrel6_t9`, `fixup_sharc_pcrel6_t10`, `fixup_sharc_addr24`,
  `fixup_sharc_imm32`, `fixup_sharc_imm16`, `fixup_sharc_imm7`,
  `fixup_sharc_imm6`. Each has a segment list
  `ArrayRef<std::pair<uint8_t,uint8_t>>` of (hi, lo) bit ranges in the
  48-bit window, listed MSB-first, taken from `decode_table.json`.

- [ ] **Step 1: Write the failing tests.** `reloc-isa.s`:

```asm
# RUN: llvm-mc -triple=sharc -mcpu=adsp-2137x -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s
# RUN: llvm-objdump -dr %t.o | FileCheck %s --check-prefix=DIS

JUMP ext_func;
CALL ext_func (DB);
R0 = ext_data;
JUMP (PC, local);
nop; nop;
local: nop;

# CHECK: R_SHARC_ADDR24_NW ext_func 0x0
# CHECK: R_SHARC_ADDR24_NW ext_func 0x0
# CHECK: R_SHARC_ADDR32_NW ext_data 0x0
# DIS: JUMP (PC, 0x3);
```

  The local `JUMP (PC, local)` resolves to 3 instructions (18 bytes / 6) with
  no relocation. `reloc-visa.s` is the same with `+visa`: `R_SHARC_PCREL24_VISA`
  for a cross-section PC-relative jump, and the local offset counted in
  parcels. `fixup-range.s` checks
  `error: fixup value out of range [-32, 31]` on an over-long `pcrel6`.
  `data.s` checks `.word sym` → `R_SHARC_DATA32`.

- [ ] **Step 2: Run and watch it fail.**

- [ ] **Step 3: Implement fixups.** In `applyFixup` (mirror Xtensa's current
  signature):
  1. Read the instruction's window with `SHARC::readWindow` from the
     fragment data at `Fixup.getOffset()`.
  2. Convert `Value`:
     - PC-relative ISA: `(Value) / 6`
     - PC-relative VISA: `Value / 2`
     - Absolute fields: leave unchanged here, because the linker applies
       the unit division.

     Assert the division is exact. Then check the range per kind.
  3. Scatter the bits across the kind's segments (the MSB segment gets the
     top bits).
  4. Write the window back with the original width.

  A local PC-relative fixup's base is the address of the instruction
  holding it. Check the PGR p136 note on PC-relative bases for VISA and
  record it in `SHARCUsage.rst`.

- [ ] **Step 4: Implement `getRelocType`.** Use the "Address units" table
  from Task 1. Whether the section is VISA (needed to pick `_ISA` or
  `_VISA`, and `_NW` or `_SW`) comes from the fixup's fragment section
  flags (`SHF_SHARC_VISA`, set in Task 12). Until Task 12 lands, key it on
  `STI.hasFeature(FeatureVISA)`, and add a test in Task 12 that covers
  mixed sections.

- [ ] **Step 5: Run the tests.** Expected: PASS. Fill in the "Relocations"
  section of `SHARCUsage.rst` with one row per relocation: field, unit,
  formula, range.

- [ ] **Step 6: Commit**

```bash
git add llvm/lib/Target/SHARC llvm/test/MC/SHARC llvm/docs/SHARCUsage.rst
git commit -m "[SHARC] Add fixups and ELF relocations"
```

---

### Task 12: ISA/VISA sections, directives and e_flags

**Files:**
- Create: `MCTargetDesc/SHARCTargetStreamer.{h,cpp}`
- Modify: `SHARCAsmParser.cpp` (`.isa`/`.visa`, the `v` section flag via `parseSectionFlags` hook or `ELFAsmParser` extension), `SHARCELFObjectWriter.cpp` / `SHARCMCTargetDesc.cpp` (set e_flags from the CPU), `SHARCDisassembler.cpp` (choose the mode from the section)
- Modify: `llvm/lib/MC/MCParser/ELFAsmParser.cpp` (accept the `v` flag when `Triple::sharc`, mapping it to `SHF_SHARC_VISA`)
- Modify: `llvm/tools/llvm-objdump/llvm-objdump.cpp` (only if needed: pass the section flag to the disassembler, see Step 3)
- Test: `llvm/test/MC/SHARC/{section-visa-flag.s,directives.s,eflags.s,mixed-sections.s,invalid-visa.s}`

- [ ] **Step 1: Write the failing tests.** `section-visa-flag.s`:

```asm
# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -filetype=obj %s -o %t.o
# RUN: llvm-readobj -S %t.o | FileCheck %s
# RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=DIS
.section .text.isa,"ax"
r0 = r0 + r1;
.section .text.visa,"axv"
r0 = r0 + r1;
# CHECK:      Name: .text.isa
# CHECK:      Flags [
# CHECK-NOT:    SHF_SHARC_VISA
# CHECK:      Name: .text.visa
# CHECK:        SHF_SHARC_VISA (0x10000000)
# DIS: <.text.isa>:
# DIS-NEXT: 3e 01 00 00 01 10 {{.*}}R0 = R0 + R1;
# DIS: <.text.visa>:
# DIS-NEXT: 01 c0 {{.*}}R0 = R0 + R1;
```

  Also add `SHF_SHARC_VISA` to the flag-name printing in
  `llvm/tools/llvm-readobj/ELFDumper.cpp`, the way `SHF_X86_64_LARGE` is
  printed.

  `invalid-visa.s`: `.visa` with `-mcpu=adsp-2137x` gives
  `error: .visa requires the visa feature`; a Type 10a instruction in a
  `.visa` section gives `error: Type 10 forms are not available in VISA code`.

  `eflags.s`: `-mcpu=adsp-2156x`, checked with `llvm-readobj -h`, shows
  `Flags [ (0x4)`.

  `mixed-sections.s`: relocations in an ISA and a VISA section of the same
  object get `_ISA` and `_VISA` types respectively.

- [ ] **Step 2: Run and watch it fail.**

- [ ] **Step 3: Implement.**
  - The parser keeps the current section's mode in the target streamer
    (`SHARCTargetStreamer::setVISA(bool)`). Section switches update it from
    the section's flags. `.visa` / `.isa` set the flag on the current
    section (`MCSectionELF::setFlags`).
  - The default for `.text` on a `+visa` CPU is VISA.
  - The code emitter, form selection and fixup kinds read the mode from the
    streamer, or from the fragment's section in the backend.
  - In the disassembler, `llvm-objdump` exposes the section through
    `SectionedAddress`, but not its flags. Select the mode per
    section like this: add a SHARC hook through `llvm-objdump`'s per-section loop, which already creates
    per-section STI copies for some targets (search for `getFeatures` and
    `SecondarySTI` in `llvm-objdump.cpp`). Use a secondary STI with the
    feature `+visa-section` for sections flagged `SHF_SHARC_VISA`. If that
    hook is not acceptable, fall back to a target-specific
    `MCDisassembler::setSectionFlags(uint64_t)` virtual; propose it in the
    eventual upstream review.
  - `FeatureVISASection` (`"visa-section"`) is an internal feature that only
    selects decoding mode.

- [ ] **Step 4: Run the tests.** Expected: PASS. Re-run all SHARC tests.

- [ ] **Step 5: Commit**

```bash
git add -A llvm/lib/Target/SHARC llvm/lib/MC/MCParser/ELFAsmParser.cpp llvm/tools/llvm-objdump \
  llvm/tools/llvm-readobj/ELFDumper.cpp llvm/test/MC/SHARC
git commit -m "[SHARC] Add ISA/VISA sections, directives and e_flags"
```

---

### Task 13: Relaxation, loop-end widening and loop mode

**Files:**
- Create: `AsmParser/SHARCLoopTracker.{h,cpp}`
- Modify: `SHARCAsmParser.cpp` (buffering, `onLabelParsed`, `flushPendingInstructions`), `SHARCAsmBackend.cpp` (`mayNeedRelaxation`, `fixupNeedsRelaxationAdvanced`, `relaxInstruction`, `writeNopData`)
- Test: `llvm/test/MC/SHARC/{relax-visa.s,loop-widen-214xx.s,loop-widen-sharcplus.s,loop-short.s,loop-mode-f.s,invalid-loop.s}`

**Interfaces:**
- Consumes: `SHARCWideForms.def`, `SHARCShortToCompute.def`, `selectForm`.
- Produces:
  - `class SHARCLoopTracker { void onDo(MCSymbol *End, size_t DoIdx, bool ExplicitMode); void onLabel(MCSymbol*); bool anyOpen() const; ... };`
  - `MCInst widenTo48(const MCInst&)` (in `SHARCMCTargetDesc.cpp`, shared by
    the parser and the backend)

- [ ] **Step 1: Write the failing tests.** `loop-widen-214xx.s`:

```asm
# RUN: llvm-mc -triple=sharc -mcpu=adsp-214xx -mattr=+visa -show-encoding %s | FileCheck %s
.visa
LCNTR = 10, DO end UNTIL LCE;
r0 = r0 + r1;          # 5th from end: stays 16-bit
r0 = r0 + r1;          # 4th from end: widened
r0 = r0 + r1;
r0 = r0 + r1;
end: r0 = r0 + r1;
r0 = r0 + r1;          # after loop: 16-bit
# CHECK: encoding: [0x01,0xc0]
# CHECK: R0 = R0 + R1;  # encoding: [0x3e,0x01,0x00,0x00,0x01,0x10]
# CHECK: R0 = R0 + R1;  # encoding: [0x3e,0x01,0x00,0x00,0x01,0x10]
# CHECK: R0 = R0 + R1;  # encoding: [0x3e,0x01,0x00,0x00,0x01,0x10]
# CHECK: R0 = R0 + R1;  # encoding: [0x3e,0x01,0x00,0x00,0x01,0x10]
# CHECK: encoding: [0x01,0xc0]
```

  The widened `R0 = R0 + R1` is Type 2a TRUE with ALU 00000001, rn=0 rx=0
  ry=1, i.e. window `0x013E00001001` → bytes `3e 01 00 00 01 10`. Confirm it
  with `llvm_vectors.llvm_check("Type2a", 48, cond=0x1F, compute=0x001001)`.

  `loop-widen-sharcplus.s`: the same idea with 12 body instructions under
  `-mcpu=adsp-2156x`. The last 11 are widened.

  `loop-short.s`: a 3-instruction loop on 214xx is fully widened.

  `loop-mode-f.s` (`adsp-2156x`), in three parts:
  - A loop whose last 11 instructions contain `JUMP` must get `(F)` clear
    (E2-active) and print as `DO end UNTIL LCE;`.
  - A loop with no branch or IDLE gets `(F)`.

    Confirm which way round is correct on PRM p137 before writing the
    expectations; the research summary said the bit is set by checking for
    a branch or IDLE.
  - An explicit `(F)` in the source is kept.

  `relax-visa.s`: `JUMP (PC, far)` with `far` 40 parcels away relaxes from
  `pcrel6` (Type 9b) to `pcrel24` (Type 8a).

  `invalid-loop.s`:
  - a `DO` whose end label never appears in the section gives
    `error: hardware loop end 'x' is not defined in this section`;
  - a directive inside the last N instructions gives
    `error: directive inside the last 4 instructions of a hardware loop`.

- [ ] **Step 2: Run and watch it fail.**

- [ ] **Step 3: Implement the buffer.**
  - While `LoopTracker.anyOpen()`, `matchAndEmitInstruction` appends the
    built `MCInst` (with its `SMLoc`) to `Pending` instead of emitting it.
  - `onLabelParsed(Sym)` → `LoopTracker.onLabel(Sym)` records that the
    **next** instruction is the last one of that loop.
  - When that instruction arrives:
    1. Widen `Pending[i]` for the last N with `widenTo48` (via
       `SHARCWideForms.def`; for `T2c → T2a` also convert the short compute
       with `SHARCShortToCompute.def`, setting cond = TRUE).
    2. On `sharcplus`, set the DO instruction's mode bit, unless it was
       explicit.
    3. Close the loop. If no loop is open, flush `Pending` to the streamer.
  - `flushPendingInstructions(MCStreamer&)` is called by the generic parser
    before directives and at end of file. If a loop is open, flush anyway,
    and report the directive error from Step 1 if the directive falls within
    the last N. Track the instruction count since the DO; the exact
    position is known only once the end label is seen. So report it when
    the end label shows that one of the flushed instructions should have
    been widened.

- [ ] **Step 4: Implement relaxation.** `mayNeedRelaxation` is true for VISA
  forms with an expression operand whose narrow field could overflow:
  `pcrel6`, `imm16`, `imm7`. `relaxInstruction` replaces the instruction
  with its `SHARCWideForms.def` partner, or the long-branch form (Type 9b
  rel → Type 8a rel). Instructions widened for a loop end are never
  narrowed again.

- [ ] **Step 5: Run the tests.** Expected: PASS. Re-run all SHARC tests.

- [ ] **Step 6: Commit**

```bash
git add llvm/lib/Target/SHARC llvm/test/MC/SHARC
git commit -m "[SHARC] Widen hardware loop ends, infer SHARC+ loop mode, relax VISA forms"
```

---

### Task 14: Checker

**Files:**
- Create: `AsmParser/SHARCChecker.{h,cpp}`
- Modify: `SHARCAsmParser.cpp` (run the checker on each instruction as it is emitted from the buffer)
- Test: `llvm/test/MC/SHARC/{invalid-delay-slot.s,invalid-loop-branch.s,invalid-double-write.s,warn-predictor.s}`

**Interfaces:**
- Produces: `class SHARCChecker { void check(const MCInst&, SMLoc, const LoopContext&); };`
  It reports through `MCAsmParser::Error`/`Warning`. `-sharc-predictor-warnings`
  (a `cl::opt`, default off) turns on the PRM p118 warnings.

- [ ] **Step 1: Write the failing tests.** Each line has its expected
  diagnostic:

```asm
# RUN: not llvm-mc -triple=sharc -mcpu=adsp-2137x %s 2>&1 | FileCheck %s
JUMP (PC, 10) (DB);
nop;
JUMP (PC, 4);
# CHECK: :[[@LINE-1]]:1: error: branch in the delay slot of a delayed branch
IF EQ JUMP (PC, 10) (DB);
IF NE JUMP (PC, 4);
nop;
# CHECK-NOT: :[[@LINE-3]]:{{.*}}error
R0 = DM(I0, M0), R0 = PM(I8, M8);
# CHECK: :[[@LINE-1]]:1: error: both memory moves write R0
```

  The mutually exclusive EQ/NE pair is allowed (PGR p141).
  `invalid-loop-branch.s`: a `JUMP` in the last 3 instructions of a loop
  gives `error: JUMP or CALL in the last 3 instructions of a hardware loop`.
  `warn-predictor.s`: with `-sharc-predictor-warnings`, two branches inside
  one 48-bit window in VISA give a warning.

- [ ] **Step 2: Run and watch it fail.**

- [ ] **Step 3: Implement.** The checker keeps a two-entry window after any
  branch with `MOD_DB`. "Mutually exclusive" means both conditions are the
  same base condition, one with bit 4 set and one without. The loop context
  comes from `SHARCLoopTracker`: the index within the loop, counted from
  the end.

- [ ] **Step 4: Run the tests.** Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add llvm/lib/Target/SHARC llvm/test/MC/SHARC
git commit -m "[SHARC] Diagnose delay-slot, loop-end and double-write hazards"
```

---

### Task 15: Blind transcription and the A-vs-C comparison

The blind agent must never see digiemu's JSON tables, the generated `.td`,
or `SHARCFormSlots.def`. **Dispatch it as a fresh subagent with only this
task's Step 1 text and the two PDF paths. Do not give it this plan.**

**Files:**
- Create (blind agent, in `~/GitHub/digiemu/tools/sharcspec/blind/`, a fresh directory): `blind_forms.json`, `blind_compute.json`, `blind_notes.md`
- Create (main agent): `~/GitHub/digiemu/tools/sharcspec/compare_td.py`, `tests/test_sharcspec_compare.py`
- Modify: `llvm/docs/SHARCUsage.rst` ("Manual errata")
- Modify: generated `.td` files in LLVM, by hand, for each resolved discrepancy

- [ ] **Step 1: Dispatch the blind transcription.** Prompt for the fresh
  agent, given verbatim:

  > Using only `~/Documents/adsp-2136x_2137x_214xx_pgr_rev2.4.pdf` (PGR) and `~/Documents/sc58x-2158x-prm.pdf` (PRM), and no other files in `~/GitHub` (do not read any repository), transcribe every SHARC instruction format (Types 1a–26a with their b/c/d variants) into `~/GitHub/digiemu/tools/sharcspec/blind/blind_forms.json` as a list of `{name, width, features: ["classic"|"visa"|"sharcplus"], mask, value, fields: [{label, hi, lo}], pages: [..]}`, where `mask`/`value`/`hi`/`lo` use bit numbers of a 48-bit window with the instruction left-aligned at bit 47. Separately transcribe the computation opcode tables (PGR ch. 12, PRM ch. 18) into `blind_compute.json` as `{table, opcode_pattern, syntax, fields, pages}`. Where the PGR and PRM disagree, record both and the page numbers in `blind_notes.md`; do not resolve. Render the figure pages to PNG (pymupdf) and read them visually: PRM figure digits in white cells are placeholders, gray cells are fixed bits.

- [ ] **Step 2: Write the comparison test.**

```python
# tests/test_sharcspec_compare.py
import subprocess, sys
from pathlib import Path
SPEC = Path(__file__).resolve().parents[1] / "tools" / "sharcspec"

def test_compare_runs_and_reports(tmp_path):
    out = tmp_path / "report.md"
    r = subprocess.run([sys.executable, str(SPEC / "compare_td.py"),
                        "--td-json", str(tmp_path / "td.json"), "--report", str(out), "--self-test"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    assert "self-test: 0 differences" in out.read_text()
```

- [ ] **Step 3: Implement `compare_td.py`.**
  1. Run `llvm-tblgen --dump-json` on `SHARC.td` (path given by
     `--llvm-root`).
  2. For each `T*` def, rebuild `mask`/`value`/fields in 48-bit window
     coordinates from `Inst` (fixed bits) and the operand-to-bit mapping.
  3. Match it to a blind form by width + mask/value overlap, then report:
     forms missing on either side, fixed-bit differences, and field
     boundary or label differences.
  4. Do the same for compute defs against `blind_compute.json`.
  5. `--self-test` compares the `.td` with itself, to check the
     normalization round trip.

- [ ] **Step 4: Run the comparison and settle each difference.** For each
  reported difference:
  1. Open the cited manual page images.
  2. Decide which side is right.
  3. Fix the LLVM `.td` (and digiemu's table, if digiemu was wrong; tell the
     user).
  4. Add a row to "Manual errata" in `SHARCUsage.rst`: manual, page, what it
     says, what LLVM uses, and why.
  5. Re-run until 0 unresolved.

- [ ] **Step 5: Check the blind agent's errata against digiemu's.** Compare
  `blind_notes.md` with the "PRM errata" table in digiemu's
  `tools/sharcspec/README.md` (10 entries). Record in the errata section
  how many the blind pass found independently. If it missed more than 2,
  re-run Step 1 for the forms involved with a fresh agent before trusting
  the agreements.

- [ ] **Step 6: Run all SHARC lit tests** (the `.td` changed). Expected:
  PASS, after updating any expectations that relied on a corrected
  encoding. Each changed expectation needs an errata row.

- [ ] **Step 7: Commit (LLVM) and stage (digiemu).**

```bash
git add llvm/lib/Target/SHARC llvm/docs/SHARCUsage.rst llvm/test/MC
git commit -m "[SHARC] Resolve encoding discrepancies from independent transcription"
cd ~/GitHub/digiemu && git add tools/sharcspec/compare_td.py tools/sharcspec/blind tests/test_sharcspec_compare.py
```

---

### Task 16: Three-way encoding sweep (digiemu, local)

**Files:**
- Create: `~/GitHub/digiemu/tools/sharc_llvm_sweep.py`, `~/GitHub/digiemu/tests/test_sharc_llvm_sweep.py`

**Interfaces:**
- Produces: `sharc_llvm_sweep.py --llvm-bin DIR --mode {isa,visa} [--cpu C] [--samples N]`,
  which writes a summary (per-form agree/disagree counts) and, on
  disagreement, the 48-bit word with the three decodings. These are
  synthetic words, not firmware, so they can be printed.

- [ ] **Step 1: Write the failing test.**

```python
def test_sweep_small(tmp_path):
    import subprocess, sys, os
    bin_dir = os.environ.get("SHARC_LLVM_BIN", os.path.expanduser("~/GitHub/llvm-project/build-sharc/bin"))
    r = subprocess.run([sys.executable, "tools/sharc_llvm_sweep.py", "--llvm-bin", bin_dir,
                        "--mode", "isa", "--cpu", "adsp-2137x", "--samples", "200",
                        "--out", str(tmp_path / "s.json")], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
```

- [ ] **Step 2: Implement.**
  1. For each form, enumerate every field narrower than 8 bits; for wider
     fields, sample N random values plus 0, all-ones and the sign boundary.
  2. Build bytes with `llvm_vectors`.
  3. Decode with three tools:
     - `llvm-mc --disassemble -show-inst` (form = the `MCInst` opcode name,
       fields from the operands, mapped back using the same
       `LABEL_OPERANDS` table);
     - `sharc_decode.Decoder(mode=...)`;
     - pypcode with the generated SLEIGH language (copy the measure path
       `tools/sharcpcode.py` uses).
  4. Compare form names (via `llvm_name`) and field values.
  5. Separately, reassemble the LLVM text with `llvm-mc -show-encoding` and
     compare the bytes.
  6. Exit 1 on any disagreement.

- [ ] **Step 3: Run the full sweep** for the ISA classic, VISA 214xx and
  VISA SHARC+ modes. For each disagreement, find the source (LLVM, digiemu
  decoder, or SLEIGH) and fix it with a test in that codebase: a new lit
  line for LLVM, pytest for digiemu. Repeat until clean.

- [ ] **Step 4: Stage digiemu and commit LLVM fixes.** Commit LLVM fixes
  one per bug, each with its lit test: `[SHARC] Fix <form> <what>`.

---

### Task 17: Firmware corpus round trip (digiemu, local only)

**Files:**
- Create: `~/GitHub/digiemu/tools/sharc_llvm_corpus.py`

**Interfaces:**
- Produces: `sharc_llvm_corpus.py --llvm-bin DIR --trace TRACE --image IMG`,
  which prints **aggregate counts only**: executed words, decoded by LLVM,
  undocumented-form words (by digiemu's classification), round-trip
  mismatches, and per-form coverage. It never prints addresses, bytes or
  disassembly text.

- [ ] **Step 1: Find the inputs.** Read digiemu's `CLAUDE.md`, `tools/sharc_trace.py`
  and `docs/STATUS.md` to find how to produce an execution trace of the
  SHARC+ image. Ask the user before running the emulator, per digiemu's
  rule "Run the emulator only when a static answer is not enough, and bound
  runs with `--limit`". Check `sections/.source-sha256` against the `.syx`
  as `CLAUDE.md` requires.

- [ ] **Step 2: Implement.**
  1. Collect the unique executed instruction addresses from the trace, and
     their words from the image (in memory only).
  2. Batch-disassemble with `llvm-mc --disassemble -mcpu=adsp-2156x`, in a
     VISA section context (`-mattr=+visa,+visa-section`).
  3. Reassemble and compare bytes.
  4. Words that digiemu's decoder labels `*undoc*` are counted separately
     and not required to decode.
  5. Exit 1 if any documented-form word fails to decode or round-trip.

- [ ] **Step 3: Run it.** For each failure class (reported as a form name
  and count), reproduce it with a **synthetic** word built by
  `llvm_vectors` with the same form and field shape. Add that synthetic
  word as a lit test, then fix the bug. Firmware bytes are never put into
  a test.

- [ ] **Step 4: Repeat for classic SHARC** if digiemu has a classic-SHARC
  image (Digitone mk1). Ask the user whether one exists and which CPU it
  is.

- [ ] **Step 5: Record the counts.** Put the aggregate numbers in the M1
  completion note (Task 18). Stage the digiemu script.

---

### Task 18: M1 completion audit

**Files:**
- Modify: `llvm/docs/SHARCUsage.rst` (remove the in-progress markers, add a "Validation" section with aggregate results)
- Modify: `docs/superpowers/specs/2026-10-04-sharc-backend-design.md` (mark M1 done, and record any deviations)

- [ ] **Step 1: Check each completion criterion** from spec §6, with
  evidence:
  1. **Every form and compute opcode has a lit test.** Write a script that
     lists `SHARC::T*`/`C_*`/`SC_*` defs from `--dump-json` and greps the
     tests for each def's expected encoding or name; it prints the missing
     ones. Expected: none missing.
  2. **A vs C:** `compare_td.py` reports 0 unresolved.
  3. **Three-way sweep:** clean in all three modes (Task 16 output).
  4. **Corpus:** 100% of documented-form words decode and round-trip
     (Task 17 counts).
  5. **Addressing:** the Task 1 decision is documented and implemented
     (Task 11 tests pass).

- [ ] **Step 2: Run the full test suites.**
  Run: `build-sharc/bin/llvm-lit -v llvm/test/MC/SHARC llvm/test/MC/Disassembler/SHARC llvm/test/tools/llvm-readobj/ELF/ && ninja -C build-sharc check-llvm-unit`
  Expected: all PASS. Also run `ninja -C build-sharc check-llvm-mc` to make
  sure no other target regressed, since `ELFAsmParser.cpp` and
  `llvm-objdump` changed.

- [ ] **Step 3: Update the docs and commit.**

```bash
git add llvm/docs/SHARCUsage.rst docs/superpowers/specs/2026-10-04-sharc-backend-design.md
git commit -m "[SHARC] Complete M1: MC layer validated"
```
