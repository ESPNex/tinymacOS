/* SPDX-License-Identifier: MIT
 * Synchronous image-source call on a source-owned supervisor stack.
 * Caller holds fs_gate; dependencies form an immutable DAG. Nothing about
 * interrupt entry, the scheduler, USB or input paths is replaced here.
 * SysV: rdi=stack top, rsi=function, rdx=argument, rax=result.
 */
.text
.global image_source_stack_call
.type image_source_stack_call,@function
image_source_stack_call:
    push %r12
    mov %rsp,%r12
    mov %rdi,%rsp
    and $-16,%rsp
    mov %rdx,%rdi
    call *%rsi
    mov %r12,%rsp
    pop %r12
    ret
.size image_source_stack_call,.-image_source_stack_call
.section .note.GNU-stack,"",@progbits
