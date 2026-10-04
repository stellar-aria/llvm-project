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
*SHARC Processor Programming Reference* (Rev 2.4, "PGR") and *SHARC+ Core
Programming Reference* (Rev 1.5, "PRM"). Page numbers are physical PDF pages.

The target is experimental. Only the MC layer (assembler, disassembler and
object files) is implemented.

Processors and features
=======================

============== ============================================== ==============================
Feature        Meaning                                        Processors
============== ============================================== ==============================
(base)         48-bit ISA, SIMD, 40-bit data registers        ``adsp-2136x``, ``adsp-2137x``
``visa``       16/32/48-bit VISA encodings                    ``adsp-214xx``
``sharcplus``  SHARC+ core; implies the three below           ``adsp-2156x``, ``adsp-sc58x``,
                                                              ``adsp-2158x``
``byte-addr``  Byte address space modifiers, ``B2W``/``W2B``  SHARC+
``fp64``       64-bit floating point on register pairs        SHARC+
============== ============================================== ==============================

Object file layout
==================

SHARC objects are ``ELFCLASS32``, ``ELFDATA2LSB``, ``EM_SHARC`` (133).
``e_flags`` bits 0-3 identify the processor family: 1 = ADSP-2136x,
2 = ADSP-2137x, 3 = ADSP-214xx, 4 = SHARC+.

Instructions are stored as 16-bit parcels, most significant parcel first;
each parcel is little-endian. A 48-bit instruction ``0x013E00001012`` is
stored as the bytes ``3e 01 00 00 12 10``; a 32-bit VISA instruction is the
top two parcels and a 16-bit one the top parcel. This matches the order in
which the instruction alignment buffer consumes parcels (PGR p125).

Data is little-endian: a 32-bit data word is stored as an ordinary
little-endian 32-bit value.

Code sections linked into short-word space (VISA code) carry
``SHF_SHARC_VISA`` (``0x10000000``). Sections without it hold ISA code, which
runs from normal-word space. Execution changes between the two only by
branching (PGR p123-136), so a section is never mixed.

Address units
=============

ELF addresses (``sh_addr``, ``st_value``, ``r_addend``) are byte addresses.
Instruction fields hold word addresses in one of several aliased spaces, so
each relocation converts a byte address to a word address.

Memory is organized in blocks of four 16-bit columns. Normal-word (NW, 32
bits), short-word (SW, 16 bits) and long-word (LW, 64 bits) addresses alias
the same storage with fixed column selection (PGR p296-301): within a block
with normal-word base ``B``, NW address ``n``, SW address ``2n`` and LW
address ``n/2`` name the same location.

48-bit instructions and 40-bit data in NW space pack three columns per word
with no gaps: the first row of NW 48-bit address ``n`` is ``(n - B) * 3/4``
rows into the block (PGR p300, PRM p228-229). The PRM writes "3/2"
but describes the same operation (``x + (x << 1)``, then ``>> 2``); see
`Manual errata`_. The 48-bit word ``n`` therefore starts at SW address
``2B + 3(n - B)``.

The byte address of a location is defined per memory block:

* **Classic SHARC:** ``byte = 2 * SW``. Classic processors have no byte
  address space; this is the canonical alias used for ELF addresses.
* **SHARC+:** the hardware byte-space address. Within a block, byte-space
  addresses are affine in SW addresses with a per-block offset, which comes
  from the processor's data sheet memory map. The PRM states that ``W2B``
  and ``B2W`` are "likely" shifts by two but that the exact semantics
  depend on the address map (PRM Table 6-4, p201).

So every conversion has the form ``word = Wbase + (S + A - Bbase) / unit``,
where ``Bbase`` and ``Wbase`` are the byte and word base of the memory block
containing ``S + A``. The linker takes them from a per-processor block table
(milestone M2):

=================== ====================== ===============================
Field address kind  ``unit`` (bytes)       Word base for the block
=================== ====================== ===============================
NW 32-bit data      4                      ``B``
SW (incl. VISA PC)  2                      ``2B``
LW                  8                      ``B / 2``
NW 48-bit code      6                      ``B`` (the ISA instruction
                                           address)
Byte space          1                      (none; field = ``S + A``)
=================== ====================== ===============================

For NW 48-bit code the block must start the 48-bit range at a 64-bit
boundary, which the PGR's wrapping rules already require (PGR p298-299).

PC-relative fields count instructions in ISA code (6 bytes each) and
parcels in VISA code (2 bytes each, since the VISA PC is an SW address, PRM
p118), so PC-relative fixups within one section are resolved by the
assembler without block bases.

Relocations
===========

*In progress (milestone M1, task 11).*

Manual errata
=============

*In progress (milestone M1, task 15).*

.. list-table::
   :header-rows: 1

   * - Manual
     - Page
     - Manual says
     - LLVM uses
     - Reason
   * - PRM
     - p229
     - 48-bit address "multiplied by 3/2"
     - 3/4
     - The PRM's own description of the operation (add the address to its
       left-shifted version, shift right by two) is 3/4, matching PGR p300.
   * - PGR
     - Table 10-2 (p473)
     - Ureg code 0x7D is "LRPTL"
     - LIRPTL
     - Typographical error; the register is LIRPTL everywhere else.
   * - PGR
     - Table 10-1 (p18117 text, 10-2)
     - EMU and IDL rows of Type 22a contradict each other
     - emu = 1 is EMUIDLE
     - PRM Type 22a table (17-13) is unambiguous.
   * - PRM
     - 14-16 (Type 3b BH table)
     - No (LW) row
     - l/x/w = 1/1/1 is (LW)
     - The PGR's Type 3b syntax (9-12) lists (LW); l/x/w = 0/1/1 is the plain
       access in the same table.
   * - PRM
     - 14-46 (Type 7a figure)
     - No M register field
     - m at bits 29-27
     - The Type 7a syntax uses Mb; field position from the PGR grid.
   * - PRM
     - 17-7 (Type 20a)
     - The table labelled CACHE lists push/pop STS; DMCACHE gives
       l1di = l1dwb = 1 as "writeback"
     - fc is FLUSH CACHE; l1di = l1dwb = 1 is FLUSH DM_CACHE
     - The STS table repeats the same rows; PMCACHE gives "flush" for the
       same bits.
