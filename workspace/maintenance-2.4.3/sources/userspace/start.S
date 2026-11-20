.section .start,"ax"
.global _start
_start: cld
 andq $-16,%rsp
 call app_main
 mov $5,%rax
 xor %edi,%edi
 syscall
 ud2
.section .note.GNU-stack,"",@progbits
