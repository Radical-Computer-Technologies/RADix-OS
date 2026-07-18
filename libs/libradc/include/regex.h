#ifndef RAD_SYSROOT_REGEX_H
#define RAD_SYSROOT_REGEX_H

#include <stddef.h>

typedef struct { const char *pattern; int cflags; size_t re_nsub; } regex_t;
typedef long regoff_t;
typedef struct { regoff_t rm_so; regoff_t rm_eo; } regmatch_t;

#define REG_EXTENDED 1
#define REG_ICASE 2
#define REG_NOSUB 4
#define REG_NEWLINE 8
#define REG_NOTBOL 16
#define REG_NOTEOL 32
#define REG_STARTEND 64
#define REG_NOMATCH 1

int regcomp(regex_t *preg, const char *regex, int cflags);
int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size);
void regfree(regex_t *preg);

#endif
