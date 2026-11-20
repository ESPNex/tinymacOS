.text
.global load_gdt
load_gdt:
 lgdt (%rdi)
 pushq $8
 lea 1f(%rip),%rax
 push %rax
 lretq
1: mov $16,%eax
 mov %ax,%ds
 mov %ax,%es
 mov %ax,%ss
 xor %eax,%eax
 mov %ax,%fs
 mov %ax,%gs
 mov $40,%eax
 ltr %ax
 ret
.global sys_entry
sys_entry:
 movq %rsp,%gs:16
 movq %gs:8,%rsp
 pushq $27
 pushq %gs:16
 push %r11
 pushq $35
 push %rcx
 pushq $0
 pushq $128
 jmp common
.altmacro
.macro ISR n
.global isr\n
isr\n:
 .if (\n != 8) && (\n != 10) && (\n != 11) && (\n != 12) && (\n != 13) && (\n != 14) && (\n != 17) && (\n != 21) && (\n != 29) && (\n != 30)
 pushq $0
 .endif
 pushq $\n
 jmp common
.endm
.set N,0
.rept 256
 ISR %N
 .set N,N+1
.endr
common:
 cld
 push %rax
 push %rbx
 push %rcx
 push %rdx
 push %rbp
 push %rsi
 push %rdi
 push %r8
 push %r9
 push %r10
 push %r11
 push %r12
 push %r13
 push %r14
 push %r15
 mov %rsp,%rdi
 andq $-16,%rsp
 call dispatch
 mov %rax,%rsp
 jmp restore
.global enter_frame
enter_frame:
 mov %rdi,%rsp
restore:
 pop %r15
 pop %r14
 pop %r13
 pop %r12
 pop %r11
 pop %r10
 pop %r9
 pop %r8
 pop %rdi
 pop %rsi
 pop %rbp
 pop %rdx
 pop %rcx
 pop %rbx
 pop %rax
 add $16,%rsp
 iretq
.section .rodata
.balign 8
.global vectors
vectors:
.macro PTR n
 .quad isr\n
.endm
.set N,0
.rept 256
 PTR %N
 .set N,N+1
.endr
.section .note.GNU-stack,"",@progbits
