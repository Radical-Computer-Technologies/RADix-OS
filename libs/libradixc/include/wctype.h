#ifndef RADIX_SYSROOT_WCTYPE_H
#define RADIX_SYSROOT_WCTYPE_H

#include <wchar.h>

typedef int (*wctype_t)(wchar_t);
typedef int (*wctrans_t)(wchar_t);

int iswspace(wchar_t wc);
int iswalpha(wchar_t wc);
int iswalnum(wchar_t wc);
int iswblank(wchar_t wc);
int iswdigit(wchar_t wc);
int iswgraph(wchar_t wc);
int iswlower(wchar_t wc);
int iswprint(wchar_t wc);
int iswpunct(wchar_t wc);
int iswcntrl(wchar_t wc);
int iswupper(wchar_t wc);
int iswxdigit(wchar_t wc);
int towlower(wchar_t wc);
int towupper(wchar_t wc);
wctype_t wctype(const char *name);
int iswctype(wint_t wc, wctype_t desc);
wctrans_t wctrans(const char *name);
wint_t towctrans(wint_t wc, wctrans_t desc);

#endif
