/* QA/opt-in static userspace layout. Same high address and W^X load segments
 * as the OS, with explicit retained constructor boundaries. Not a dynamic ELF
 * loader or automatic process-entry ABI change for existing OS applications. */
ENTRY(_start)
PHDRS { code PT_LOAD FLAGS(5); ro PT_LOAD FLAGS(4); rw PT_LOAD FLAGS(6); }
SECTIONS {
 . = 0x8000001000;
 .text : { *(.start) *(.text*) } :code
 . = ALIGN(4096);
 .rodata : { *(.rodata*) } :ro
 . = ALIGN(8);
 .init_array : {
  __tmc_init_array_start = .;
  KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*))) KEEP(*(.init_array))
  __tmc_init_array_end = .;
 } :ro
 . = ALIGN(4096);
 .data : { *(.data*) *(.got*) } :rw
 .bss : { *(.bss*) *(COMMON) } :rw
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
