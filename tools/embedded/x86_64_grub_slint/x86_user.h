#ifndef RAD_X86_64_GRUB_SLINT_USER_H
#define RAD_X86_64_GRUB_SLINT_USER_H

#include <radixkernel/radixkernel.h>

typedef struct x86_user_context {
    uint64_t kernel_rsp;
    uint64_t entry;
    uint64_t stack_top;
    uint64_t exit_reason;
    int32_t pid;
    uint32_t reserved;
    uint64_t resume_rip;
    uint64_t resume_rsp;
    uint64_t resume_rflags;
    uint64_t fork_rax;
} x86_user_context_t;

typedef struct x86_user_trap_frame {
    uint64_t rax;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t r10;
    uint64_t r8;
    uint64_t r9;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t user_rsp;
    uint64_t ss;
} x86_user_trap_frame_t;

extern "C" int x86_user_run_init(const char *path);
extern "C" rad_status_t x86_user_spawn_process(const char *path, int32_t parent_pid, int32_t *pid_out, rad_task_t *task_out);
extern "C" rad_status_t x86_user_spawn_process_with_stdio(const char *path, int32_t parent_pid, const char *stdio_path, int32_t *pid_out, rad_task_t *task_out);
extern "C" int x86_user_wait_process(int32_t pid);
extern "C" int x86_user_copy_self_test(void);
extern "C" void x86_enter_user_context(x86_user_context_t *context);
extern "C" void x86_user_set_active_context(x86_user_context_t *context);
extern "C" x86_user_context_t *x86_user_active_context(void);
extern "C" long x86_syscall_dispatch_frame(x86_user_trap_frame_t *frame);
extern "C" long x86_syscall_dispatch(unsigned long number, unsigned long arg0, unsigned long arg1,
                                      unsigned long arg2, unsigned long arg3, unsigned long arg4,
                                      unsigned long arg5);

#endif
