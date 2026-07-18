#ifndef RAD_SYSROOT_WCHAR_H
#define RAD_SYSROOT_WCHAR_H

#include <stddef.h>

#ifndef _RAD_WCHAR_T_DEFINED
#define _RAD_WCHAR_T_DEFINED
typedef int wchar_t;
#endif
typedef unsigned long wint_t;
typedef struct { unsigned int state; } mbstate_t;

#define WEOF ((wint_t)-1)

wint_t btowc(int c);
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
size_t mbrlen(const char *s, size_t n, mbstate_t *ps);
int mbsinit(const mbstate_t *ps);
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps);
int wctomb(char *s, wchar_t wc);
int wcwidth(wchar_t wc);
size_t wcslen(const wchar_t *s);
size_t wcsnlen(const wchar_t *s, size_t maxlen);
wchar_t *wcscat(wchar_t *dest, const wchar_t *src);
wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n);
wchar_t *wmempcpy(wchar_t *dest, const wchar_t *src, size_t n);
wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n);
void mbszero(mbstate_t *ps);
size_t wcsrtombs(char *dest, const wchar_t **src, size_t len, mbstate_t *ps);
size_t mbsrtowcs(wchar_t *dest, const char **src, size_t len, mbstate_t *ps);
size_t mbstowcs(wchar_t *dest, const char *src, size_t n);
size_t wcstombs(char *dest, const wchar_t *src, size_t n);

#endif
