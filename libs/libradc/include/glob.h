#ifndef RAD_SYSROOT_GLOB_H
#define RAD_SYSROOT_GLOB_H

#include <stddef.h>
#include <sys/stat.h>

struct dirent;

typedef struct {
    size_t gl_pathc;
    char **gl_pathv;
    size_t gl_offs;
    int gl_flags;

    void (*gl_closedir)(void *);
    struct dirent *(*gl_readdir)(void *);
    void *(*gl_opendir)(const char *);
    int (*gl_lstat)(const char *, struct stat *);
    int (*gl_stat)(const char *, struct stat *);
} glob_t;

#define GLOB_ERR        (1 << 0)
#define GLOB_MARK       (1 << 1)
#define GLOB_NOSORT     (1 << 2)
#define GLOB_DOOFFS     (1 << 3)
#define GLOB_NOCHECK    (1 << 4)
#define GLOB_APPEND     (1 << 5)
#define GLOB_NOESCAPE   (1 << 6)
#define GLOB_PERIOD     (1 << 7)
#define GLOB_MAGCHAR    (1 << 8)
#define GLOB_ALTDIRFUNC (1 << 9)
#define GLOB_BRACE      (1 << 10)
#define GLOB_NOMAGIC    (1 << 11)
#define GLOB_TILDE      (1 << 12)
#define GLOB_ONLYDIR    (1 << 13)
#define GLOB_TILDE_CHECK (1 << 14)

#define __GLOB_FLAGS (GLOB_ERR | GLOB_MARK | GLOB_NOSORT | GLOB_DOOFFS | \
    GLOB_NOCHECK | GLOB_APPEND | GLOB_NOESCAPE | GLOB_PERIOD | \
    GLOB_MAGCHAR | GLOB_ALTDIRFUNC | GLOB_BRACE | GLOB_NOMAGIC | \
    GLOB_TILDE | GLOB_ONLYDIR | GLOB_TILDE_CHECK)

#define GLOB_NOSPACE 1
#define GLOB_ABORTED 2
#define GLOB_NOMATCH 3

int glob(const char *pattern, int flags, int (*errfunc)(const char *epath, int eerrno), glob_t *pglob);
void globfree(glob_t *pglob);
int glob_pattern_p(const char *pattern, int quote);

#endif
