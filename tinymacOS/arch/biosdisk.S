/* Authored build glue; vendor binary remains the pinned official artifact. */
.section .rodata
.balign 512
.global bios_boot_mbr,bios_duet,bios_duet_end
bios_boot_mbr:
.incbin TMC_MBR_PATH
.balign 512
bios_duet:
.incbin "vendor/opencore-1.0.8/bootX64-blockio"
bios_duet_end:
.section .note.GNU-stack,"",@progbits
