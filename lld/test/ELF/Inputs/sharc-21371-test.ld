/* ADSP-21371 internal RAM (data sheet Table 3); ELF byte = 2 * SW. */
SECTIONS {
  .text   0x00260000 : { *(.text) }
  .text2  0x00260030 : { *(.text2) }
  .data   0x002E0000 : { *(.data) }
}
