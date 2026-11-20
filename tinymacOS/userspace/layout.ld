ENTRY(_start)
PHDRS { code PT_LOAD FLAGS(5); ro PT_LOAD FLAGS(4); rw PT_LOAD FLAGS(6); }
SECTIONS { . = 0x8000001000; .text : { *(.start) *(.text*) } :code . = ALIGN(4096); .rodata : { *(.rodata*) } :ro . = ALIGN(4096); .data : { *(.data*) *(.got*) } :rw .bss : { *(.bss*) *(COMMON) } :rw /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) } }
