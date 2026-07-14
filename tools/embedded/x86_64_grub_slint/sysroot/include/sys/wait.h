#ifndef RADIX_SYSROOT_SYS_WAIT_H
#define RADIX_SYSROOT_SYS_WAIT_H

typedef int pid_t;

#define WNOHANG 1
#define WEXITSTATUS(status) ((status) & 0xff)
#define WIFEXITED(status) (1)
#define WIFSIGNALED(status) (0)

pid_t waitpid(pid_t pid, int *wstatus, int options);
pid_t wait(int *wstatus);

#endif
