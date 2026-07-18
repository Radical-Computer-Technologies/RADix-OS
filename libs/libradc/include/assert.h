#ifndef RAD_SYSROOT_ASSERT_H
#define RAD_SYSROOT_ASSERT_H

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
void __rad_assert_fail(const char *expr, const char *file, int line);
#define assert(expr) ((expr) ? (void)0 : __rad_assert_fail(#expr, __FILE__, __LINE__))
#endif

#endif
