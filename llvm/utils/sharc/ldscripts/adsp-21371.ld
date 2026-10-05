/* Default link layout for the ADSP-21371 (classic SHARC). ELF addresses are
   2 * SW (ADSP-21371/21375 data sheet Rev. D, Table 3, internal RAM).
   The processor has no VISA. 48-bit code must start on a 48-bit row of its
   block: ALIGN cannot express 6, so the start is rounded with arithmetic. */
ENTRY(_start)

MEMORY {
  B0 (rx) : ORIGIN = 0x00260000, LENGTH = 0x8000
  B1 (rw) : ORIGIN = 0x002E0000, LENGTH = 0x8000
  B2 (rw) : ORIGIN = 0x00300000, LENGTH = 0x8000
  B3 (rw) : ORIGIN = 0x00380000, LENGTH = 0x8000
}

SECTIONS {
  .text : {
    . = ORIGIN(B0) + ((. - ORIGIN(B0) + 5) / 6) * 6;
    *(.text .text.*)
  } > B0
  .rodata : { *(.rodata .rodata.*) } > B1
  .data : { *(.data .data.*) } > B1
  .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) } > B1
  __stack_end = ORIGIN(B1) + LENGTH(B1);
}
