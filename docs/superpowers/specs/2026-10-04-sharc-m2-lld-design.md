# SHARC M2: linking with LLD

Status: design approved in conversation, 2026-10-04
Parent: `2026-10-04-sharc-backend-design.md` (roadmap row M2)
Depends on: M1 (MC layer, relocations R_SHARC_* in `llvm/docs/SHARCUsage.rst`)

## 1. Goal and scope

LLD links SHARC objects produced by the M1 assembler into executable ELF images
for **adsp-2156x** (SHARC+) and the classic **adsp-21371** and **adsp-21375**. Word-addressed
relocation fields are resolved through a per-CPU memory block table taken from
the public data sheets. digiemu's sharcemu runs the linked 2156x images as
execution tests.

The other four CPUs (adsp-2136x, adsp-214xx, adsp-sc58x, adsp-2158x) come later
as new table rows and scripts.

Out of scope: ROM and external memory (SDRAM, DMC, SPI flash); a boot-stream
(LDR) writer; thunks and range extension; shared objects, GOT/PLT and TLS;
linker relaxation; ISA-mode execution in sharcemu; the clang driver (M5/M6).

Rules carried over from M1: clean room (public ADI manuals and data sheets
only); expected values in tests are computed by hand from the documents, never
captured from LLVM's output; no firmware in tests; digiemu changes are staged,
committed only when the user asks.

Build: `build-sharc` needs `-DLLVM_ENABLE_PROJECTS=lld`.

## 2. Sources

- ADSP-21562/3/5/6/7/9 data sheet, Rev. D (June 2023), page 8-9: Table 2
  (L1 blocks), Table 3 (L2).
- ADSP-21371/ADSP-21375 data sheet, Rev. D (April 2013), page 5-6: Table 3
  (ADSP-21371 internal memory space), Table 4 (ADSP-21375).
- SHARC+ PRM: the memory map "appears in the product-specific data sheet".

Both data sheets are in `~/Documents`.

## 3. Memory block table

### 3.1 Model

An ELF address is a byte address: the hardware byte address on SHARC+, and
`2 * SW` on classic SHARC (M1, `SHARCUsage.rst`, "Address units"). A **block**
is a byte range `[Bbase, Bbase + size)` with, for each address kind, either a
word base or "absent":

| Kind | Unit (bytes) | Used by |
|---|---|---|
| `lw` | 8 | `ADDR32_LW` |
| `nw` | 4 | `ADDR32_NW` |
| `isa` | 6 | `ADDR24_NW` (ISA code addresses) |
| `sw` | 2 | `ADDR24_SW`, `ADDR32_SW` (VISA code addresses) |
| `byte` | 1 | `ADDR32_BYTE` |

`word(kind, x) = Wbase[kind] + (x - Bbase) / unit[kind]`, valid only when
`(x - Bbase) % unit == 0` and the result is inside the kind's range. The inverse
`byte(kind, w)` serves llvm-objdump and the sharcemu loader.

### 3.2 Rows

`llvm/include/llvm/TargetParser/SHARCMemoryMap.def`, one
`SHARC_BLOCK(cpu, name, bbase, size, lw, nw, isa, sw, byte)` row per block,
`-1` for an absent kind, each with a comment naming the data sheet table.

adsp-2156x (Table 2, Table 3; byte addresses are native):

| Block | Byte | LW | ISA | NW | SW (VISA) |
|---|---|---|---|---|---|
| L1 block 0 | 0x00240000, 0x30000 | 0x48000 | 0x90000 | 0x90000 | 0x120000 |
| L1 block 1 | 0x002C0000, 0x30000 | 0x58000 | 0xB0000 | 0xB0000 | 0x160000 |
| L1 block 2 | 0x00300000, 0x20000 | 0x60000 | 0xC0000 | 0xC0000 | 0x180000 |
| L1 block 3 | 0x00380000, 0x20000 | 0x70000 | 0xE0000 | 0xE0000 | 0x1C0000 |
| L2 RAM (8 Mb) | 0x20000000, 0x100000 | absent | 0x5C0000 | 0x08000000 | 0xB80000 |

The L2 row is the 8 Mb part's range. The 2 and 4 Mb parts start L2 higher
(Table 3: byte 0x200C0000 and 0x20080000) and end at the same address. M2 uses
the 8 Mb row; the default script places nothing in L2, and placing code there
is the user's choice. A per-part CPU split is left until it is needed.

L2 has no LW view. Its "VISA address space" is in SW units (the range is
twice the NW range), and SW data access to L2 is not listed, so `ADDR32_SW`
into L2 is an error while `ADDR24_SW` (a VISA code address) is allowed. The
table therefore splits `sw` into `sw_code` and `sw_data` columns; L1 blocks
have both, L2 only `sw_code`.

adsp-21371 (Table 3; RAM only; byte = 2 * SW):

| Block | Byte (2*SW) | LW | ISA | NW | SW |
|---|---|---|---|---|---|
| block 0 RAM | 0x00260000, 0x8000 | 0x4C000 | 0x90000 | 0x98000 | 0x130000 |
| block 1 RAM | 0x002E0000, 0x8000 | 0x5C000 | 0xB0000 | 0xB8000 | 0x170000 |
| block 2 RAM | 0x00300000, 0x8000 | 0x60000 | 0xC0000 | 0xC0000 | 0x180000 |
| block 3 RAM | 0x00380000, 0x8000 | 0x70000 | 0xE0000 | 0xE0000 | 0x1C0000 |

adsp-21375 (Table 4) has the same bases with half the RAM per block: each
block is 0x4000 bytes (block 0 RAM: NW 0x98000-0x98FFF, ISA 0x90000-0x90AA9).

### 3.2.1 Processor parts in e_flags

The two classic parts differ only in RAM size, so they share
`EF_SHARC_MACH_2137X` and are told apart by a new field,
`EF_SHARC_PART = 0x000000f0`: `EF_SHARC_PART_NONE = 0`,
`EF_SHARC_PART_21371 = 0x10`, `EF_SHARC_PART_21375 = 0x20`.

- CPUs `adsp-21371` and `adsp-21375` are added with the features of
  `adsp-2137x`; the assembler sets the part field from `-mcpu`. `adsp-2137x`
  stays as a generic CPU with no part and no memory map.
- `getSHARCCPUName` returns the part name when the field is set.
- LLD: objects with no part link with objects of one part, and the output
  carries that part; two different parts are an error. A classic link where
  no input names a part is an error asking for `-mcpu=adsp-21371` or
  `-mcpu=adsp-21375`. SHARC+ objects with no part use the adsp-2156x table.

`byte` is present for every 2156x block and absent for 2137x (classic has no
byte space; `ADDR32_BYTE` is an error there).

### 3.3 API

`llvm/include/llvm/TargetParser/SHARCMemoryMap.h`:

- `enum class SHARCAddrKind { LW, NW, ISA, SWCode, SWData, Byte };`
- `const SHARCBlock *findBlock(StringRef CPU, uint64_t Byte);`
- `std::optional<uint64_t> toWord(const SHARCBlock &, SHARCAddrKind, uint64_t Byte);`
  (nullopt for absent kind, misalignment or out of range)
- `std::optional<uint64_t> toByte(StringRef CPU, SHARCAddrKind, uint64_t Word);`
- `StringRef memoryMapCPU(unsigned EFlags);` (from `EF_SHARC_MACH_*` and
  `EF_SHARC_PART_*`; "" when no table applies)

### 3.4 Correction to M1 docs

`SHARCUsage.rst` states that 48-bit word `n` starts at SW `2B + 3(n - B)`. That
assumes the ISA and NW bases coincide, which holds on SHARC+ but not on the
2137x (block 0 RAM: ISA 0x90000, NW 0x98000). The general rule is
`SW = 2 * NWbase + 3 * (n - ISAbase)`. The docs are corrected; no M1 code uses
the rule.

## 4. LLD target: `lld/ELF/Arch/SHARC.cpp`

A `TargetInfo` subclass in the style of MSP430/AVR, registered for `EM_SHARC`.

### 4.1 Relocations

- `getRelExpr`: `PCREL*` -> `R_PC`; `DATA32`, `ADDR*` -> `R_ABS`; `NONE` ->
  `R_NONE`; `IMM16/7/6` (reserved) -> error "unsupported relocation".
- `relocate(loc, rel, val)`:
  - `DATA32`: 32-bit little-endian word, `S + A`.
  - `ADDR*`: find the block containing `val` for the output CPU, convert with
    the kind of the relocation (`ADDR24_NW` -> ISA, `ADDR24_SW` -> SWCode,
    `ADDR32_NW` -> NW, `ADDR32_SW` -> SWData, `ADDR32_LW` -> LW,
    `ADDR32_BYTE` -> Byte), range-check, insert.
  - `PCREL*`: `val` is `S + A - P` in bytes; divide by 6 (ISA) or 2 (VISA),
    check exact division, signed range, and that `S + A` and `P` are in the
    same block.
- Field insertion uses the window bits of the M1 relocation table, applied to
  the instruction's 16-bit parcels (most significant parcel first, each little
  endian), sharing the bit-placement helper with `SHARCAsmBackend` via a small
  header in `llvm/include/llvm/BinaryFormat/ELFRelocs/` or TargetParser.

### 4.2 Errors

Each names the relocation type, the symbol and the input section:

1. target outside every block of the CPU;
2. the block has no view of the needed kind (e.g. `ADDR32_LW` into L2,
   `ADDR32_BYTE` on classic parts);
3. target misaligned for the unit (ISA: `(S + A - Bbase) % 6 != 0`);
4. value out of range for the field;
5. PC-relative pair crossing blocks;
6. `e_flags` machine or part mismatch between inputs, or no part on a classic
   link.

### 4.3 Output rules

- `calcEFlags`: every input has the same `EF_SHARC_MACH` (or none) and the
  same `EF_SHARC_PART` (or none); the output carries both (§3.2.1). No machine anywhere: error, since relocations need a CPU.
- `SHF_SHARC_VISA` is kept on output sections; an output section mixing VISA
  and non-VISA executable input sections is an error.
- A non-VISA executable output section must start at a multiple of 6 bytes from
  its block's ISA base (checked after layout, error names the section).
- `defaultImageBase`: byte base of L1 block 0 for the CPU, so a link without
  `-T` lands in RAM.

### 4.4 Tests: `lld/test/ELF/sharc-*.s`

- One test per relocation type on each CPU where the kind exists, linking
  across sections and blocks; values computed by hand in comments from §3.2.
- One test per error in §4.2 and per rule in §4.3.
- Checks with `llvm-objdump -d` (instruction text) and `llvm-readelf`.

## 5. Default linker scripts

`llvm/utils/sharc/ldscripts/adsp-2156x.ld`, `adsp-21371.ld` and `adsp-21375.ld`, GNU-ld syntax,
byte addresses:

- `MEMORY` region per block of §3.2.
- `.text` (ISA) in block 0 at an ISA-aligned start, rounded with
  `. = ORIGIN(b0) + ((. - ORIGIN(b0) + 5) / 6) * 6;`; VISA code sections
  (`.visa*`, flagged by the assembler) after it in block 0.
- `.data`, `.rodata`, `.bss` in block 1; `__stack_end` at the top of block 1.
- `ENTRY(_start)`.

Clang will pass them with `-T` (M5/M6). The lit tests use them explicitly, and
check placement with `llvm-readelf -S`.

## 6. llvm-objdump

For linked images (`ET_EXEC`), absolute operands of `ADDR24_*` and `ADDR32_*`
kinds are converted back to byte addresses with `toByte` and printed with their
symbol, `JUMP 0x90010 <main+0x18>`. Relocatable objects keep M1's output. The
SHARC instruction printer exposes the operand kind through
`MCInstrAnalysis::evaluateBranch` (code addresses) and a SHARC hook for data
addresses.

## 7. Execution tests (digiemu)

- `tools/sharcemu/elfload.py`: reads an `EM_SHARC` 2156x executable, puts each
  `PT_LOAD` at its byte address in `Image`, registers VISA code at its SW
  address (block table parsed from `SHARCMemoryMap.def`), sets the PC from
  `e_entry` and the stack from `__stack_end`. Rejects other machines and CPUs.
- `tools/sharc_lld_exec.py` and `tests/sharc_lld/*.s` (pytest): assemble with
  `llvm-mc`, link with `ld.lld -T adsp-2156x.ld`, run until `_done` or the
  budget, check `# EXPECT: R0 = ...` and `# EXPECT-MEM: sym = ...`.
  Expectations follow the PRM's description of each instruction.
- Programs cover: calls and jumps across sections and from L1 to L2; `DM(sym)`
  loads and stores to another block; PC-relative branches and a hardware loop
  in another section of the same block; data defined in a second object;
  `R0 = sym` then a store through it.
- Standing permission (2026-10-04): sharcemu may run these test programs
  without asking; firmware runs still need asking.

Limits: execution is VISA-only and 2156x-only. ISA linking and 2137x are
covered by lit tests.

## 8. Docs

`SHARCUsage.rst` gains a "Linking" section (block table, scripts, the errors of
§4.2 with fixes, placing ISA code), the corrected rule of §3.4, and the data
sheets in its references.

## 9. Completion criteria

1. Every R_SHARC type has an lld lit test on 2156x and 2137x (where its kind
   exists) with hand-computed values, and each error of §4.2 and rule of §4.3
   has a test.
2. `SHARCMemoryMapTest` passes: rows consistent across views (sizes agree,
   ISA ranges pack to 3/4 of the NW range), each row citing its table.
3. The default scripts link the lit tests and place sections in the intended
   blocks.
4. llvm-objdump symbolizes absolute targets in linked images.
5. All digiemu execution tests pass in sharcemu.
6. The MC, SHARC and lld test suites pass.
