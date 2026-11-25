/* Disposable QA sentinel, NOT Windows. Confirms execution of the original MBR. */
.code16
.section .text
.global _start
_start:
 ljmp $0,$go
go:
 xorw %ax,%ax
 movw %ax,%ds
 movw $message,%si
loop:
 lodsb
 testb %al,%al
 jz halt
 movb $0x0e,%ah
 movw $7,%bx
 int $0x10
 jmp loop
halt: hlt
 jmp halt
message: .asciz "\r\nPREVIOUS_BOOT_OK\r\n"
.org 440,0
