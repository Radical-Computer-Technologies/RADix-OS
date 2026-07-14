#ifndef RADIX_SYSROOT_TERM_H
#define RADIX_SYSROOT_TERM_H

#include <ncurses.h>

typedef struct _radix_terminal TERMINAL;

extern TERMINAL *cur_term;

int setupterm(char *term, int fd, int *errret);
char *termname(void);
char *tgetstr(const char *id, char **area);
char *tigetstr(const char *capname);
int tigetnum(const char *capname);
int tigetflag(const char *capname);
char *tparm(const char *str, ...);
int tputs(const char *str, int affcnt, int (*putc_fn)(int));

#endif
