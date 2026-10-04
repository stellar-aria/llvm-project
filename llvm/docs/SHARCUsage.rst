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

Immediate data
==============

The 6-bit modifier of Type 4 (``<data6>``), the 7-bit offset of Type 15b
(``<data7>``) and the 16-bit data of Types 16b and 17b (``<data16>``) are
treated as sign-extended to 32 bits. Neither manual states how these
fields are extended; the manuals call only the PC-relative offsets two's
complement (PGR p16557, p16875). The assumption agrees with MAME's ADSP-2106x
interpreter (``<data6>``) and with digiemu's SHARC+ trace tooling
(``<data7>``, ``<data16>``), and is checked against real firmware and by
execution in milestones M1 and M2. The assembler accepts only values the
field holds exactly under this rule, so ``R0 = 0xFFFF;`` uses the 48-bit
Type 17a and ``R0 = -1;`` may use the 32-bit Type 17b.

Relocations
===========

SHARC objects use RELA relocations. ``S`` is the symbol's ELF (byte)
address, ``A`` the addend and ``P`` the address of the instruction holding
the field; every relocation is placed at the start of its instruction. The
word-space conversions are those of `Address units`_: ``nw48(x)`` is the
normal-word address of the 48-bit instruction at byte address ``x``,
``sw(x)``, ``nw(x)`` and ``lw(x)`` the short-, normal- and long-word
addresses.

========================== ==== ====================== ===================== =============
Name                       Num  Field (window bits)    Value                 Range
========================== ==== ====================== ===================== =============
``R_SHARC_NONE``           0
``R_SHARC_DATA32``         1    32-bit data word       ``S + A``             32 bits
``R_SHARC_ADDR24_NW``      2    23-0                   ``nw48(S + A)``       unsigned 24
``R_SHARC_ADDR24_SW``      3    23-0                   ``sw(S + A)``         unsigned 24
``R_SHARC_ADDR32_NW``      4    31-0                   ``nw(S + A)``         32 bits
``R_SHARC_ADDR32_SW``      5    31-0                   ``sw(S + A)``         32 bits
``R_SHARC_ADDR32_LW``      6    31-0                   ``lw(S + A)``         32 bits
``R_SHARC_ADDR32_BYTE``    7    31-0                   ``S + A``             32 bits
``R_SHARC_PCREL24_ISA``    8    23-0                   ``(S + A - P) / 6``   signed 24
``R_SHARC_PCREL24_VISA``   9    23-0                   ``(S + A - P) / 2``   signed 24
``R_SHARC_PCREL23_ISA``    10   22-0                   ``(S + A - P) / 6``   signed 23
``R_SHARC_PCREL23_VISA``   11   22-0                   ``(S + A - P) / 2``   signed 23
``R_SHARC_PCREL6_ISA``     12   32, 31-27              ``(S + A - P) / 6``   signed 6
``R_SHARC_PCREL6_VISA``    13   32, 31-27              ``(S + A - P) / 2``   signed 6
``R_SHARC_IMM16``          14   reserved
``R_SHARC_IMM7``           15   reserved
``R_SHARC_IMM6``           16   reserved
========================== ==== ====================== ===================== =============

The assembler uses them as follows:

* ``JUMP label``, ``CALL label`` and ``CJUMP label`` give a 24-bit absolute
  code address: ``ADDR24_NW`` in ISA code, ``ADDR24_SW`` in VISA code.
* ``(PC, label)`` and ``DO label UNTIL ...`` are PC-relative: ``PCREL24``,
  or ``PCREL23`` for SHARC+ loops (whose bit 23 is the loop mode; on
  classic cores a loop offset is 24 bits). Within one section the
  assembler resolves them itself.
* A symbol in a 32-bit field (``DM(sym)``, ``R0 = sym``) is a normal-word
  address, ``ADDR32_NW``. Short-word, long-word and byte addresses
  (``ADDR32_SW``, ``_LW``, ``_BYTE``) are reserved for operand specifiers
  that are not implemented yet.
* ``.word sym`` gives ``DATA32``.

Symbols are not accepted in 16-, 7- or 6-bit data fields or 6-bit
PC-relative fields; the assembler uses a wider form instead.

Hardware loops
==============

For a loop written with a label, ``LCNTR = n, DO end UNTIL LCE;`` with
``end:`` on the loop's last instruction, the assembler:

* uses 48-bit encodings for the last four instructions of the loop in VISA
  code (ADSP-214xx, PGR 4-54), or the last eleven (SHARC+, PRM p6619 text),
  or all of a shorter loop. It is an error if such an instruction has no
  48-bit form, such as a byte load without ``(EX)``;
* on SHARC+, writes a counter loop as F1-active, ``(F)``, when its last eleven
  instructions hold no branch, IDLE, loop start or inner loop end, and as
  E2-active otherwise (PRM 4-41 text, "Loop Categorization"). A loop
  written with ``(F)`` keeps it.

A loop written with a numeric offset, ``DO (PC, n)``, is left as written.

A PC-relative jump to a label in VISA code starts as a 32-bit Type 9b with
a 6-bit offset and becomes the 48-bit Type 8a when the label is out of
range or in another section.

Diagnostics
===========

The assembler reports as errors the instruction sequences the manuals say
development tools should reject:

* in the two instructions after a delayed branch: a jump, call or return
  (unless its condition is the opposite of the branch's), a PC stack push
  or pop, a write to ``PCSTK`` or ``PCSTKP``, or ``IDLE`` (PGR 4-22, 4-23);
* in a loop written with a label: ``JUMP`` or ``CALL`` in the last three
  instructions; ``IDLE`` or ``EMUIDLE`` anywhere in a counter loop of one to
  three instructions, as the last of four, or as the fifth from last of a
  longer one, or in the last three of an arithmetic loop; a load of
  ``CURLCNTR`` as the fifth from last of a counter loop (PGR 4-55);
* ``IF NOT LCE`` right after a write to ``CURLCNTR`` (PGR 4-55);
* a dual move loading one register on both buses (PRM 7-28).

``-sharc-predictor-warnings`` adds warnings for the SHARC+ branch predictor
guidance (PRM 4-12): fewer than twelve 48-bit instructions' worth of code
after a branch in its section, and two branches in one 48-bit window of
VISA code.

Manual errata
=============

Many PRM encoding figures start from a template whose cells print default
digits. Where a figure's fixed digits are the unedited template, or a copy of
another figure, LLVM follows the PGR's opcode grids; the shading (which bits
are fixed) is still taken from the PRM. Encodings were also checked against
an independent transcription of both manuals.

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
   * - PGR
     - 10-27 (Type 22c grid)
     - Bit 39 (IDL) is a field
     - Bit 39 is fixed at 1
     - With IDL = 0 a Type 22c word is the Type 21c NOP; Type 22a fixes the
       same bit at 1 (PRM 17-13 figure).
   * - PRM
     - 14-16 (Type 3b BH table)
     - No (LW) row
     - l/x/w = 1/1/1 is (LW)
     - The PGR's Type 3b syntax (9-12) lists (LW); l/x/w = 0/1/1 is the plain
       access in the same table.
   * - PRM
     - 14-16 (Type 3b BHSE table)
     - No (SWSE) row
     - l/x/w = 1/1/0 is (SWSE), as in Type 4b
     - SHARC+ code uses it.
   * - PRM
     - 14-22 (Type 3d tables)
     - Only rows with EX
     - ex = 0 gives (BW), (SW), (BWSE), (SWSE) without EX
     - SHARC+ code uses Type 3d without EX where a 48-bit form is required.
   * - PRM, PGR
     - Table 12-11, Type 6a
     - The LSHIFT/ASHIFT/ROT immediate is <data8>
     - The count is DATAEX:DATA, 12-bit two's complement
     - SHARC+ code sets DATAEX to all ones for negative counts.
   * - PRM
     - 17-3 (Type 18a figure)
     - Bit 36 is neither fixed nor a field
     - Undefined; decoded with a warning
     - SHARC+ code sets it in one place; its meaning is open.
   * - PRM
     - 14-48 (Type 7a figure)
     - A copy of the Type 7d figure: breg and toby, no M field, and no
       position for the w and l bits of the BH table
     - The PGR grid (m at 29-27), plus w at bit 39 and l at bit 23 (also in
       Type 7b)
     - The Type 7a syntax uses Mb. Type 7d, "an extension (address switch) to
       7a", fixes bits 39 and 23 at 1: the w = l = 1 combination the BH table
       leaves out. The positions are inferred from this, not drawn.
   * - PRM
     - 17-7 (Type 20a)
     - The table labelled CACHE lists push/pop STS; DMCACHE gives
       l1di = l1dwb = 1 as "writeback"
     - fc is FLUSH CACHE; l1di = l1dwb = 1 is FLUSH DM_CACHE
     - The STS table repeats the same rows; PMCACHE gives "flush" for the
       same bits.
   * - PRM
     - 14-9 (Type 2a figure)
     - Fixed bits 47-38 are 0010000000 (the Type 1a template)
     - 000 at 47-45, 00001 at 44-40
     - PGR Type 2a grid (10-6).
   * - PRM
     - 14-11 (Type 2b figure)
     - Bits 47-39 are 110000000 (template)
     - 000000011
     - PGR Type 2b grid (10-6).
   * - PRM
     - 14-15 (Type 3a figure)
     - The figure is a copy of Type 1a
     - PGR Type 3a layout
     - The figure's inner caption reads Type1a.
   * - PRM
     - 14-31 (Type 4b figure)
     - Label dreg[6:0] over a 4-bit field
     - 4-bit dreg
     - Matches Types 4a and 4d and the PGR.
   * - PRM
     - 14-40, 15-12 (Types 5b, 9b)
     - VISA marker at bits 22-16 drawn 0000000
     - 0111111
     - PGR Type 5b and 9b grids.
   * - PRM
     - 15-22 (Type 11c figure)
     - Bits 47-41 are 1100000 (template)
     - 0000101
     - PGR Type 11c grid (10-18).
   * - PRM
     - 16-17 (Type 17a figure)
     - Label i[2:0] over the 7-bit field at 38-32
     - ureg[6:0]
     - The syntax and the PGR grid name a Ureg.
   * - PRM
     - 16-18 (Type 17b figure)
     - Bits 47-39 are 100100000, the Type 16b prefix
     - 000011111
     - PGR Type 17b grid (10-23).
   * - PRM
     - 17-18 (Type 25a rframe)
     - 0001 1010 0000 0100 in bits 47-32, bits 23-0 open
     - 0x190000000000
     - PGR RFRAME grid (10-29); its prefix agrees with the Type 25c RFRAME (0x1901).
   * - PRM
     - 17-19 (Type 25c figure)
     - A copy of the 32-bit Type 25a rframe figure
     - 16-bit 0001100100000001
     - PGR Type 25c grid (10-29).
   * - PRM
     - 17-7 (Type 19a bitrev)
     - Bits 41-39 are 000; Type 19a modify has sc[1:0] and w there
     - 101, as in the PGR; sc and w are not supported
     - Open: with sc and w, bitrev and modify overlap; SHARC+ semantics of sc/w are not described.
   * - PRM
     - 18-10, 18-11 (Table 18-9)
     - Shift-immediate codes 011001 for BITEXT (NU), 011011 for Rn OR FDEP
       and 011101 for Rn OR FDEP (SE)
     - 010110, 011001 and 011011
     - The PGR's rule (Table 12-11, 12-10): the Type 6 code is the upper six
       bits of the shifter opcode, which the table's other rows and its own
       shifter column (01011000, 01100100, 01101100) follow.
   * - PRM
     - 18-10, 18-11 (Table 18-9)
     - No BITDEP row
     - BITDEP Rx BY Ry and BY <bitlen12> (0111 0100, 011101)
     - PGR Table 12-11 (214xx and later).
   * - PRM
     - 18-4 (Figure 18-3)
     - dataex[3:0] drawn at bits 26-23
     - Bits 30-27 of the Type 6a word
     - The Type 6a figures of both manuals put DATAEX at 30-27.
