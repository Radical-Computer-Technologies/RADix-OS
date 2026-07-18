#ifndef RAD_SYSROOT_SYSCALLS_H
#define RAD_SYSROOT_SYSCALLS_H

#include <stddef.h>
#include <stdint.h>

#define RAD_SYS_READ 0
#define RAD_SYS_WRITE 1
#define RAD_SYS_OPEN 2
#define RAD_SYS_CLOSE 3
#define RAD_SYS_IOCTL 4
#define RAD_SYS_LSEEK 5
#define RAD_SYS_STAT 6
#define RAD_SYS_FSTAT 7
#define RAD_SYS_GETTIMEOFDAY 8
#define RAD_SYS_NANOSLEEP 9
#define RAD_SYS_EXIT 10
#define RAD_SYS_FORK 11
#define RAD_SYS_EXECVE 12
#define RAD_SYS_WAITPID 13
#define RAD_SYS_GETPID 14
#define RAD_SYS_GETPPID 15
#define RAD_SYS_DUP 16
#define RAD_SYS_DUP2 17
#define RAD_SYS_CHDIR 18
#define RAD_SYS_GETCWD 19
#define RAD_SYS_BRK 20
#define RAD_SYS_PIPE 21
#define RAD_SYS_FCNTL 22
#define RAD_SYS_ACCESS 23
#define RAD_SYS_ISATTY 24
#define RAD_SYS_SOCKET 25
#define RAD_SYS_BIND 26
#define RAD_SYS_CONNECT 27
#define RAD_SYS_LISTEN 28
#define RAD_SYS_ACCEPT 29
#define RAD_SYS_SENDTO 30
#define RAD_SYS_RECVFROM 31
#define RAD_SYS_SHUTDOWN 32
#define RAD_SYS_SETSOCKOPT 33
#define RAD_SYS_GETSOCKOPT 34
#define RAD_SYS_MMAP 35
#define RAD_SYS_MUNMAP 36
#define RAD_SYS_POLL 39
#define RAD_SYS_SETTIMEOFDAY 40
#define RAD_SYS_GETDENTS 1000
#define RAD_SYS_REMOVE 1001
#define RAD_SYS_MKDIR 1002
#define RAD_SYS_RMDIR 1003
#define RAD_SYS_RENAME 1004
#define RAD_SYS_TRUNCATE 1005
#define RAD_SYS_GETUID 1008
#define RAD_SYS_GETEUID 1009
#define RAD_SYS_GETGID 1010
#define RAD_SYS_GETEGID 1011
#define RAD_SYS_CHMOD 1014
#define RAD_SYS_LINK 1015
#define RAD_SYS_SYMLINK 1016
#define RAD_SYS_READLINK 1017
#define RAD_SYS_FSYNC 1018
#define RAD_SYS_KILL 1019
#define RAD_SYS_FCHDIR 1020
#define RAD_SYS_FTRUNCATE 1021
#define RAD_SYS_UTIME 1022
#define RAD_SYS_GETPGID 1023
#define RAD_SYS_SETPGID 1024
#define RAD_SYS_GETSID 1025
#define RAD_SYS_SETSID 1026
#define RAD_SYS_TCGETPGRP 1027
#define RAD_SYS_TCSETPGRP 1028
#define RAD_SYS_PIPE2 1029

static inline long rad_syscall6(long n, long a, long b, long c, long d, long e, long f) {
#if defined(__aarch64__)
    register long x0 asm("x0") = a;
    register long x1 asm("x1") = b;
    register long x2 asm("x2") = c;
    register long x3 asm("x3") = d;
    register long x4 asm("x4") = e;
    register long x5 asm("x5") = f;
    register long x8 asm("x8") = n;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8) : "memory");
    return x0;
#else
    register long rax asm("rax") = n;
    register long rdi asm("rdi") = a;
    register long rsi asm("rsi") = b;
    register long rdx asm("rdx") = c;
    register long r10 asm("r10") = d;
    register long r8 asm("r8") = e;
    register long r9 asm("r9") = f;
    asm volatile("int $0x80" : "+a"(rax) : "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8), "r"(r9) : "memory");
    return rax;
#endif
}

#endif
