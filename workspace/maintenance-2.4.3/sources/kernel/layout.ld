ENTRY(_kstart)
PHDRS { code PT_LOAD FLAGS(5); ro PT_LOAD FLAGS(4); rw PT_LOAD FLAGS(6); }
SECTIONS { . = 0x4000000; _ktxt = .; .text : { *(.entry) *(.text*) } :code . = ALIGN(4096); _kro = .; .rodata : { *(.rodata*) } :ro . = ALIGN(4096); _krw = .; .data : { *(.data*) } :rw .bss : { *(.bss*) *(COMMON) } :rw . = ALIGN(4096); _kend = .; /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) } }
