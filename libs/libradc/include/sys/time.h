#ifndef RAD_SYSROOT_SYS_TIME_H
#define RAD_SYSROOT_SYS_TIME_H

struct timeval {
    long tv_sec;
    long tv_usec;
};

int gettimeofday(struct timeval *tv, void *tz);
int settimeofday(const struct timeval *tv, const void *tz);

#endif
