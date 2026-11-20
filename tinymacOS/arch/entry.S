.section .entry,"ax"
.global _kstart
_kstart: cli
 cld
 lea boot_stack_end(%rip),%rsp
 xor %ebp,%ebp
 call kmain
1: hlt
 jmp 1b
.bss
.balign 4096
.skip 32768
boot_stack_end:
.section .note.GNU-stack,"",@progbits
