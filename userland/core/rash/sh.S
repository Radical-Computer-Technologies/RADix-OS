.section .text
.code64
.global _start
.type _start, @function
_start:
    mov (%rsp), %r12
    cmp $2, %r12
    jl fail

    mov 8(%rsp), %r13
    test %r13, %r13
    jz fail
    mov 16(%rsp), %r14
    test %r14, %r14
    jz fail

    leaq 8(%rsp,%r12,8), %r15
    add $8, %r15
    cmpq $0, (%r15)
    je fail

    mov $1, %rax
    mov $1, %rdi
    leaq argv_envp_message(%rip), %rsi
    mov $argv_envp_message_end - argv_envp_message, %rdx
    int $0x80

    mov $2, %rax
    mov %r14, %rdi
    mov $1, %rsi
    int $0x80
    cmp $0, %rax
    jl fail
    mov %rax, %r13

    mov $0, %rax
    mov %r13, %rdi
    leaq script_buffer(%rip), %rsi
    mov $255, %rdx
    int $0x80
    cmp $0, %rax
    jle fail

    cmpb $0x23, script_buffer(%rip)
    jne fail
    cmpb $0x21, script_buffer+1(%rip)
    jne fail

    mov $3, %rax
    mov %r13, %rdi
    int $0x80

    mov $1, %rax
    mov $1, %rdi
    leaq script_message(%rip), %rsi
    mov $script_message_end - script_message, %rdx
    int $0x80

    mov $1, %rax
    mov $1, %rdi
    leaq exit_message(%rip), %rsi
    mov $exit_message_end - exit_message, %rdx
    int $0x80

    mov $60, %rax
    xor %rdi, %rdi
    int $0x80

fail:
    mov $1, %rax
    mov $1, %rdi
    leaq fail_message(%rip), %rsi
    mov $fail_message_end - fail_message, %rdx
    int $0x80
    mov $60, %rax
    mov $1, %rdi
    int $0x80
1:
    jmp 1b

.section .rodata
argv_envp_message:
    .ascii "RAD_USER_ARGV_ENVP_OK\n"
argv_envp_message_end:
script_message:
    .ascii "RAD_USER_SH_SCRIPT_OK\n"
script_message_end:
exit_message:
    .ascii "RAD_USER_RADSH_EXIT_OK\n"
exit_message_end:
fail_message:
    .ascii "RAD_USER_SH_SCRIPT_FAIL\n"
fail_message_end:

.section .bss
.align 16
script_buffer:
    .skip 256
