/* Native dual-boot MBR: relocate before chainloading the saved, unmodified MBR.
 * The installer patches two distinctive 64-bit LBAs; disk signature, partition
 * table and active flags stay exactly as they were. All payload lives in the
 * newly allocated native extent. No writes to existing filesystem boot sectors. */
.code16
.section .text
.global _start
_start:
 cli
 xorw %ax,%ax
 movw %ax,%ds
 movw %ax,%es
 movw %ax,%ss
 movw $0x7c00,%sp
 cld
 movw $0x7c00,%si
 movw $0x600,%di
 movw $220,%cx
 rep movsw
 ljmp $0,$relocated
relocated:
 sti
 movb %dl,drive
 movw $message,%si
print:
 lodsb
 testb %al,%al
 jz choose
 movb $0x0e,%ah
 movw $7,%bx
 int $0x10
 jmp print
choose:
 xorw %ax,%ax
 int $0x16
 cmpb $'1',%al
 je original
 cmpb $'2',%al
 je tiny
 cmpb $13,%al
 jne choose
tiny:
 movw $DUET_SECTORS,remaining
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
 call read
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
original:
 movw old_lba,%ax
 movw %ax,dap_lba
 movw old_lba+2,%ax
 movw %ax,dap_lba+2
 movw $0,dap_segment
 movw $0x7c00,dap_offset
 movw $1,dap_count
 call read
 jc error
 movb drive,%dl
 ljmp $0,$0x7c00
read:
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
 ret
error:
 cli
 hlt
 jmp error
.balign 4
dap: .byte 16,0
dap_count: .word 0
dap_offset: .word 0
dap_segment: .word 0x2020
dap_lba: .quad 0x1badb002aabbccdd
old_lba: .quad 0x1badb003aabbccdd
remaining: .word 0
chunk: .word 0
drive: .byte 0
message: .asciz "\r\n1 Previous system   2 tinymacOS [Enter]\r\n"
.org 440,0
