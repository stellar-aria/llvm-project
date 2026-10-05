/* Default link layout for the ADSP-2156x (SHARC+). ELF addresses are byte
   addresses (ADSP-21562/3/5/6/7/9 data sheet Rev. D, Table 2 and Table 3).
   ISA (48-bit) code must start on a 48-bit row of its block: ALIGN cannot
   express 6, so the start is rounded with arithmetic. */
ENTRY(_start)

MEMORY {
  L1B0 (rx)  : ORIGIN = 0x00240000, LENGTH = 0x30000
  L1B1 (rw)  : ORIGIN = 0x002C0000, LENGTH = 0x30000
  L1B2 (rw)  : ORIGIN = 0x00300000, LENGTH = 0x20000
  L1B3 (rw)  : ORIGIN = 0x00380000, LENGTH = 0x20000
  L2   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x100000
}

SECTIONS {
  .text : {
    . = ORIGIN(L1B0) + ((. - ORIGIN(L1B0) + 5) / 6) * 6;
    *(.text .text.*)
  } > L1B0
  .vtext : { *(.vtext .vtext.*) } > L1B0
  .l2text : { *(.l2text .l2text.*) } > L2
  .rodata : { *(.rodata .rodata.*) } > L1B1
  .data : { *(.data .data.*) } > L1B1
  .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) } > L1B1
  __stack_end = ORIGIN(L1B1) + LENGTH(L1B1);
}
