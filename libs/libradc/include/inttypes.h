#ifndef RAD_SYSROOT_INTTYPES_H
#define RAD_SYSROOT_INTTYPES_H

#include <stdint.h>

#define PRId64 "ld"
#define PRIu64 "lu"
#define PRIx64 "lx"

typedef struct { long quot; long rem; } imaxdiv_t;
typedef long intmax_t;
typedef unsigned long uintmax_t;

intmax_t strtoimax(const char *nptr, char **endptr, int base);
uintmax_t strtoumax(const char *nptr, char **endptr, int base);

#endif
