#ifndef RADIX_SYSROOT_SYS_IOCTL_H
#define RADIX_SYSROOT_SYS_IOCTL_H

#include <stdint.h>

#define RADIX_IOCTL_NONE 0u
#define RADIX_IOCTL_WRITE 1u
#define RADIX_IOCTL_READ 2u
#define RADIX_IOCTL_READWRITE 3u
#define RADIX_IOCTL(dir, type, nr, size) ((((uint32_t)(dir) & 3u) << 30) | (((uint32_t)(size) & 0x3fffu) << 16) | (((uint32_t)(type) & 0xffu) << 8) | ((uint32_t)(nr) & 0xffu))
#define RADIX_IOR(type, nr, type_name) RADIX_IOCTL(RADIX_IOCTL_READ, type, nr, sizeof(type_name))
#define RADIX_IOW(type, nr, type_name) RADIX_IOCTL(RADIX_IOCTL_WRITE, type, nr, sizeof(type_name))

struct termios;

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#define TIOCGWINSZ RADIX_IOR('Y', 1u, struct winsize)
#define TIOCSWINSZ RADIX_IOW('Y', 2u, struct winsize)
#define RADIX_TTY_FLUSH_INPUT 1u
#define RADIX_TTY_FLUSH_OUTPUT 2u
#define RADIX_TTY_GET_TERMIOS RADIX_IOR('Y', 5u, struct termios)
#define RADIX_TTY_SET_TERMIOS RADIX_IOW('Y', 6u, struct termios)
#define RADIX_TTY_FLUSH RADIX_IOW('Y', 7u, uint32_t)

int ioctl(int fd, unsigned long request, ...);

#endif
