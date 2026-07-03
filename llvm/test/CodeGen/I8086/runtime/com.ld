/* DOS .COM: single segment, code/data start at offset 0x100 */
ENTRY(main)
SECTIONS {
  . = 0x100;
  .text : { *(.text) *(.text.*) *(.rodata) *(.rodata.*) *(.data) *(.data.*) }
  .bss  : { *(.bss) *(COMMON) }
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame) }
}
