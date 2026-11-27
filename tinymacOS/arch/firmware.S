/* SysV supervisor wrapper. EFI callback is also SysV and calls firmware using
 * the EFI Microsoft ABI. Caller holds the BSP-only runtime lock with IF=0. */
.text
.global firmware_invoke
.type firmware_invoke,@function
firmware_invoke:
 push %rbp
 push %rbx
 push %r12
 push %r13
 push %r14
 push %r15
 mov %cr3,%r12
 mov %rsp,%r13
 mov %rsi,%r11
 mov %rdi,%r10
 mov %rdx,%rsp
 and $-16,%rsp
 mov %rcx,%rdi
 mov %r8,%rsi
 mov %r10,%cr3
 call *%r11
 cli
 cld
 mov %r12,%cr3
 mov %r13,%rsp
 pop %r15
 pop %r14
 pop %r13
 pop %r12
 pop %rbx
 pop %rbp
 ret
.size firmware_invoke,.-firmware_invoke
.section .note.GNU-stack,"",@progbits
