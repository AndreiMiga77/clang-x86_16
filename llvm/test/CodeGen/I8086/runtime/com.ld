/* DOS .COM: single segment, code/data start at offset 0x100.  Execution begins
   at the first byte (0x100), so `main` must come first -- place its section (from
   -ffunction-sections) ahead of everything else. */
ENTRY(main)
SECTIONS {
  . = 0x100;
  .text : {
    *(.text.main)
    *(.text .text.*)
    *(.rodata .rodata.*)
    *(.data .data.*)
  }
  .bss  : { *(.bss .bss.*) *(COMMON) }
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame) }
}
