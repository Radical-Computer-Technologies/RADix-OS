.section .text
.code64
.global _start
.type _start, @function
_start:
    sub $192, %rsp

    mov $39, %rax
    int $0x80
    cmp $1, %rax
    jl fail

    mov $79, %rax
    mov %rsp, %rdi
    mov $128, %rsi
    int $0x80
    cmp $0, %rax
    jl fail

    mov $0, %rax
    mov $0, %rdi
    leaq 160(%rsp), %rsi
    mov $1, %rdx
    int $0x80
    cmp $0, %rax
    jl fail

    mov $1, %rax
    mov $1, %rdi
    leaq abi_message(%rip), %rsi
    mov $abi_message_end - abi_message, %rdx
    int $0x80

    mov $1, %rax
    mov $1, %rdi
    leaq shell_message(%rip), %rsi
    mov $shell_message_end - shell_message, %rdx
    int $0x80

    mov $0, %rax
    mov $0, %rdi
    leaq 160(%rsp), %rsi
    mov $1, %rdx
    int $0x80
    cmp $1, %rax
    jne clean_exit

    mov $1, %rax
    mov $1, %rdi
    leaq live_message(%rip), %rsi
    mov $live_message_end - live_message, %rdx
    int $0x80

    movb $0x50, cow_byte(%rip)
    mov $11, %rax
    int $0x80
    cmp $0, %rax
    je fork_child
    cmp $0, %rax
    jl fail

    mov %rax, %r12
    mov $1, %rax
    mov $1, %rdi
    leaq fork_parent_message(%rip), %rsi
    mov $fork_parent_message_end - fork_parent_message, %rdx
    int $0x80

    mov $13, %rax
    mov %r12, %rdi
    xor %rsi, %rsi
    xor %rdx, %rdx
    int $0x80
    cmp %r12, %rax
    jne fail

    mov $1, %rax
    mov $1, %rdi
    leaq fork_wait_message(%rip), %rsi
    mov $fork_wait_message_end - fork_wait_message, %rdx
    int $0x80

    cmpb $0x50, cow_byte(%rip)
    jne fail

    mov $1, %rax
    mov $1, %rdi
    leaq cow_parent_message(%rip), %rsi
    mov $cow_parent_message_end - cow_parent_message, %rdx
    int $0x80

    leaq script_path(%rip), %rax
    mov %rax, (%rsp)
    movq $0, 8(%rsp)
    mov $12, %rax
    leaq script_path(%rip), %rdi
    mov %rsp, %rsi
    xor %rdx, %rdx
    int $0x80
    cmp $0, %rax
    jl fail

    mov $1, %rax
    mov $1, %rdi
    leaq exit_message(%rip), %rsi
    mov $exit_message_end - exit_message, %rdx
    int $0x80

clean_exit:
    mov $60, %rax
    xor %rdi, %rdi
    int $0x80

fork_child:
    movb $0x43, cow_byte(%rip)
    mov $1, %rax
    mov $1, %rdi
    leaq fork_child_message(%rip), %rsi
    mov $fork_child_message_end - fork_child_message, %rdx
    int $0x80

    mov $1, %rax
    mov $1, %rdi
    leaq fork_fd_message(%rip), %rsi
    mov $fork_fd_message_end - fork_fd_message, %rdx
    int $0x80

    mov $60, %rax
    mov $7, %rdi
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
abi_message:
    .ascii "RAD_USER_LINUX_SYSCALL_ABI_OK\n"
abi_message_end:
shell_message:
    .ascii "RAD_USER_RADSH_BOOT_OK\n"
shell_message_end:
live_message:
    .ascii "RAD_SLINT_WM_PTY_TERMINAL_OK\n"
live_message_end:
fork_parent_message:
    .ascii "RAD_USER_FORK_OK\n"
fork_parent_message_end:
fork_child_message:
    .ascii "RAD_USER_FORK_CHILD_OK\n"
fork_child_message_end:
fork_fd_message:
    .ascii "RAD_USER_FORK_FD_INHERIT_OK\n"
fork_fd_message_end:
fork_wait_message:
    .ascii "RAD_USER_FORK_WAIT_OK\n"
fork_wait_message_end:
cow_parent_message:
    .ascii "RAD_USER_COW_PARENT_ISOLATED_OK\n"
cow_parent_message_end:
exit_message:
    .ascii "RAD_USER_RADSH_EXIT_OK\n"
exit_message_end:
fail_message:
    .ascii "RAD_USER_RADSH_FAIL\n"
fail_message_end:
script_path:
    .asciz "/bin/test.sh"

.section .data
.align 4096
cow_byte:
    .byte 0
