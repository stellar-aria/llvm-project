/* Fixed placement for the SHARC relocation tests (ADSP-2156x byte
   addresses, data sheet Table 2 and Table 3). */
SECTIONS {
  .text    0x00240000 : { *(.text) }
  .text2   0x00240030 : { *(.text2) }
  .text3   0x00240600 : { *(.text3) }
  .vtext   0x00240800 : { *(.vtext) }
  .vtext2  0x00240840 : { *(.vtext2) }
  .data    0x002C0000 : { *(.data) }
  .l2text  0x20000000 : { *(.l2text) }
  .l2data  0x20000100 : { *(.l2data) }
}
