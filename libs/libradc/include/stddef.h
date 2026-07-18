#ifndef RAD_SYSROOT_STDDEF_H
#define RAD_SYSROOT_STDDEF_H

#define NULL ((void*)0)
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#ifndef _RAD_WCHAR_T_DEFINED
#define _RAD_WCHAR_T_DEFINED
typedef int wchar_t;
#endif
typedef union {
    long long __ll;
    long double __ld;
    void *__p;
} max_align_t;

#define offsetof(type, member) __builtin_offsetof(type, member)

#endif
