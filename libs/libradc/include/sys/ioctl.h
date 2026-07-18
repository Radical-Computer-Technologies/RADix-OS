#ifndef RAD_SYSROOT_SYS_IOCTL_H
#define RAD_SYSROOT_SYS_IOCTL_H

#include <stdint.h>

#define RAD_IOCTL_NONE 0u
#define RAD_IOCTL_WRITE 1u
#define RAD_IOCTL_READ 2u
#define RAD_IOCTL_READWRITE 3u
#define RAD_IOCTL(dir, type, nr, size) ((((uint32_t)(dir) & 3u) << 30) | (((uint32_t)(size) & 0x3fffu) << 16) | (((uint32_t)(type) & 0xffu) << 8) | ((uint32_t)(nr) & 0xffu))
#define RAD_IOR(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_READ, type, nr, sizeof(type_name))
#define RAD_IOW(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_WRITE, type, nr, sizeof(type_name))

struct termios;

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#define TIOCGWINSZ RAD_IOR('Y', 1u, struct winsize)
#define TIOCSWINSZ RAD_IOW('Y', 2u, struct winsize)
#define RAD_TTY_FLUSH_INPUT 1u
#define RAD_TTY_FLUSH_OUTPUT 2u
#define RAD_TTY_GET_TERMIOS RAD_IOR('Y', 5u, struct termios)
#define RAD_TTY_SET_TERMIOS RAD_IOW('Y', 6u, struct termios)
#define RAD_TTY_FLUSH RAD_IOW('Y', 7u, uint32_t)

int ioctl(int fd, unsigned long request, ...);

#endif
