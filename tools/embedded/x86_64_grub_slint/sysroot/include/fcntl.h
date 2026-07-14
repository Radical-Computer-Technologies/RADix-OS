#ifndef RADIX_SYSROOT_FCNTL_H
#define RADIX_SYSROOT_FCNTL_H

#define O_RDONLY 1
#define O_WRONLY 2
#define O_RDWR 3
#define O_ACCMODE 3
#define O_CREAT 4
#define O_TRUNC 8
#define O_APPEND 16
#define O_DIRECTORY 32
#define O_SEARCH 64
#define O_NOCTTY 128
#define O_NONBLOCK 256
#define O_CLOEXEC 512
#define O_NOFOLLOW 1024
#define O_EXCL 2048

#define FD_CLOEXEC 1
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_DUPFD_CLOEXEC 5

#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100

int open(const char *pathname, int flags, ...);
int openat(int dirfd, const char *pathname, int flags, ...);
int fcntl(int fd, int cmd, ...);

#endif
