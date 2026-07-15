.section .text
.code64
.global _start
.type _start, @function
_start:
    sub $160, %rsp

    mov $37, %rax
    leaq shm_name(%rip), %rdi
    mov $4096, %rsi
    mov $3, %rdx
    int $0x80
    cmp $0, %rax
    jl fail
    mov %rax, %r12

    mov $35, %rax
    xor %rdi, %rdi
    mov $4096, %rsi
    mov $3, %rdx
    mov $1, %r10
    mov %r12, %r8
    xor %r9, %r9
    int $0x80
    cmp $0, %rax
    jl fail
    mov %rax, %r13

    mov %r13, %rdi
    mov $0xff31d0aa, %eax
    mov $256, %ecx
    rep stosl

    mov $1, %rax
    mov $1, %rdi
    leaq pattern_message(%rip), %rsi
    mov $pattern_message_end - pattern_message, %rdx
    int $0x80

    mov $2, %rax
    leaq compositor_path(%rip), %rdi
    mov $2, %rsi
    int $0x80
    cmp $0, %rax
    jl fail
    mov %rax, %r14

    movl $40, 0(%rsp)
    movl %r12d, 4(%rsp)
    movl $16, 8(%rsp)
    movl $16, 12(%rsp)
    movl $16, 16(%rsp)
    movl $620, 20(%rsp)
    movl $96, 24(%rsp)
    movl $30, 28(%rsp)
    movl $0, 32(%rsp)
    movl $0, 36(%rsp)

    mov $4, %rax
    mov %r14, %rdi
    mov $0xc0284301, %rsi
    mov %rsp, %rdx
    int $0x80
    cmp $0, %rax
    jl fail
    mov 36(%rsp), %r15d
    test %r15d, %r15d
    jz fail

    movl $28, 64(%rsp)
    movl %r15d, 68(%rsp)
    movl $0, 72(%rsp)
    movl $0, 76(%rsp)
    movl $16, 80(%rsp)
    movl $16, 84(%rsp)
    movl $0, 88(%rsp)

    mov $4, %rax
    mov %r14, %rdi
    mov $0x401c4302, %rsi
    leaq 64(%rsp), %rdx
    int $0x80
    cmp $0, %rax
    jl fail

    movl $28, 96(%rsp)
    movl %r15d, 100(%rsp)
    movl $620, 104(%rsp)
    movl $96, 108(%rsp)
    movl $16, 112(%rsp)
    movl $16, 116(%rsp)
    movl $1, 120(%rsp)

    mov $4, %rax
    mov %r14, %rdi
    mov $0x401c4302, %rsi
    leaq 96(%rsp), %rdx
    int $0x80
    cmp $0, %rax
    jl fail

    mov $1, %rax
    mov $1, %rdi
    leaq done_message(%rip), %rsi
    mov $done_message_end - done_message, %rdx
    int $0x80

    mov $10, %rax
    xor %rdi, %rdi
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
shm_name:
    .asciz "/radgfx-smoke"
compositor_path:
    .asciz "/dev/compositor0"
pattern_message:
    .ascii "RADIX_SHM_USER_PATTERN_OK\n"
pattern_message_end:
done_message:
    .ascii "RADIX_SHM_PROCESS_SUBMIT_OK\n"
done_message_end:
fail_message:
    .ascii "RADIX_SHM_PROCESS_FAIL\n"
fail_message_end:
