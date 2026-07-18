#ifndef RAD_SYSROOT_SYS_UTIME_H
#define RAD_SYSROOT_SYS_UTIME_H

#include <sys/types.h>

struct utimbuf {
    time_t actime;
    time_t modtime;
};

int utime(const char *filename, const struct utimbuf *times);

#endif
