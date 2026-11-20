/* SPDX-License-Identifier: MIT
 * tinymacOS installed-disk MBR bridge to the official OpenDuet image.
 * GPT headers/entries occupy LBA 1..33; our reserved pre-ESP gap carries
 * the contiguous, pinned OpenDuet BlockIO image at LBA 64. No FAT32 PBR
 * is needed: OpenDuet's own filesystem driver reads the existing FAT16 ESP.
 * Assemble with DUET_LBA and DUET_SECTORS supplied by bios-image.py.
 * Match upstream boot1f32: file at linear 0x20200, entry 2000:0200.
 */
.code16
.section .text
.global _start
_start:
 ljmp $0,$entry
entry:
 cli
 xorw %ax,%ax
 movw %ax,%ss
 movw $0x7c00,%sp
 movw %ax,%ds
 movw %ax,%es
 cld
 sti
 movb %dl,drive
 movw $0x55aa,%bx
 movb $0x41,%ah
 int $0x13
 jc error
 cmpw $0xaa55,%bx
 jne error
 testw $1,%cx
 jz error
read_next:
 movw remaining,%ax
 testw %ax,%ax
 jz ready
 cmpw $64,%ax
 jbe chunk_ready
 movw $64,%ax
chunk_ready:
 movw %ax,chunk
 movw %ax,dap_count
 pusha
 pushw %ds
 pushw %es
 movw $dap,%si
 movb drive,%dl
 movb $0x42,%ah
 int $0x13
 popw %es
 popw %ds
 popa
 jc error
 movw chunk,%ax
 subw %ax,remaining
 addw %ax,dap_lba
 adcw $0,dap_lba+2
 shlw $5,%ax
 addw %ax,dap_segment
 jmp read_next
ready:
 movb drive,%dl
 ljmp $0x2000,$0x0200
error:
 movw $message,%si
print:
 lodsb
 testb %al,%al
 jz halt
 movb $0x0e,%ah
 movw $7,%bx
 int $0x10
 jmp print
halt:
 cli
 hlt
 jmp halt
.balign 4
dap:
 .byte 16,0
dap_count: .word 0
 .word 0
dap_segment: .word 0x2020
dap_lba: .quad DUET_LBA
remaining: .word DUET_SECTORS
chunk: .word 0
drive: .byte 0
message: .asciz "tinymacOS BIOS: disk read error"
/* The installer writes the disk signature/protective partition table itself.
 * Do not put code or state into MBR bytes 440..511. */
.org 440,0
