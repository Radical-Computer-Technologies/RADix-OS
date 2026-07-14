#ifndef RADIX_SYSROOT_MATH_H
#define RADIX_SYSROOT_MATH_H

#define HUGE_VAL (__builtin_huge_val())
#define NAN (__builtin_nanf(""))

double floor(double x);
double ceil(double x);
double fabs(double x);
int signbit(double x);

#endif
