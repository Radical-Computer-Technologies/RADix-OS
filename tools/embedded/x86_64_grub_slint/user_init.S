.section .text
.code64
.global _start
.type _start, @function
_start:
    sub $160, %rsp
    mov $5000000, %rcx
1:
    pause
    loop 1b

    mov $14, %rax
    int $0x80
    cmp $1, %rax
    jne fail

    mov $8, %rax
    mov %rsp, %rdi
    int $0x80
    test %rax, %rax
    jne fail

    mov $19, %rax
    leaq 32(%rsp), %rdi
    mov $64, %rsi
    int $0x80
    test %rax, %rax
    jne fail

    mov $1, %rax
    mov $1, %rdi
    mov $0x1000, %rsi
    mov $1, %rdx
    int $0x80
    cmp $-2, %rax
    jne fail

    mov $1, %rax
    mov $1, %rdi
    leaq invalid_ptr_message(%rip), %rsi
    mov $invalid_ptr_message_end - invalid_ptr_message, %rdx
    int $0x80

    mov $1, %rax
    mov $1, %rdi
    leaq syscall_message(%rip), %rsi
    mov $syscall_message_end - syscall_message, %rdx
    int $0x80

    mov $1, %rax
    mov $1, %rdi
    leaq init_message(%rip), %rsi
    mov $init_message_end - init_message, %rdx
    int $0x80

    leaq radsh_path(%rip), %rcx
    mov %rcx, (%rsp)
    leaq selftest_arg(%rip), %rcx
    mov %rcx, 8(%rsp)
    movq $0, 16(%rsp)
    mov $12, %rax
    leaq radsh_path(%rip), %rdi
    mov %rsp, %rsi
    int $0x80

fail:
    mov $1, %rax
    mov $1, %rdi
    leaq fail_message(%rip), %rsi
    mov $fail_message_end - fail_message, %rdx
    int $0x80
    mov $10, %rax
    mov $1, %rdi
    int $0x80
1:
    jmp 1b

.section .rodata
invalid_ptr_message:
    .ascii "RADIX_USER_INVALID_PTR_OK\n"
invalid_ptr_message_end:
syscall_message:
    .ascii "RADIX_USER_SYSCALLS_OK\n"
syscall_message_end:
init_message:
    .ascii "RADIX_USER_INIT_OK\n"
init_message_end:
fail_message:
    .ascii "RADIX_USER_SYSCALLS_FAIL\n"
fail_message_end:
radsh_path:
    .asciz "/bin/radsh"
selftest_arg:
    .asciz "--selftest"
