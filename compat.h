
#pragma once

/*
* this file serves as an ad-hoc impl for strict ansi c ('gcc -ansi ...').
* it may be incorrect; strict ansi isn't officially a target.
* it makes no attempts to handle anything other than 64bits!
*/

#if (__STRICT_ANSI__ == 1)
    
    #define FP_NAN       0
    #define FP_INFINITE  1
    #define FP_ZERO      2
    #define FP_SUBNORMAL 3
    #define FP_NORMAL    4

    #define fpclassify(x) \
    ( \
        (sizeof(x) == sizeof(float)) ? __fpclassifyf(x) : \
        ((sizeof(x) == sizeof(double)) ? __fpclassify(x) : \
        __fpclassifyl(x)) \
    )

    #define isinf(x) ( \
        sizeof(x) == sizeof(float) ? (__FLOAT_BITS(x) & 0x7fffffff) == 0x7f800000 : \
        sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & -1ULL>>1) == 0x7ffULL<<52 : \
        __fpclassifyl(x) == FP_INFINITE)

    #define isnan(x) ( \
        sizeof(x) == sizeof(float) ? (__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000 : \
        sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52 : \
        __fpclassifyl(x) == FP_NAN)

    #define isnormal(x) ( \
        sizeof(x) == sizeof(float) ? ((__FLOAT_BITS(x)+0x00800000) & 0x7fffffff) >= 0x01000000 : \
        sizeof(x) == sizeof(double) ? ((__DOUBLE_BITS(x)+(1ULL<<52)) & -1ULL>>1) >= 1ULL<<53 : \
        __fpclassifyl(x) == FP_NORMAL)

    #define isfinite(x) ( \
        sizeof(x) == sizeof(float) ? (__FLOAT_BITS(x) & 0x7fffffff) < 0x7f800000 : \
        sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & -1ULL>>1) < 0x7ffULL<<52 : \
        __fpclassifyl(x) > FP_INFINITE)

    static inline unsigned long long __DOUBLE_BITS(double __f)
    {
        union {double __f; unsigned long long __i;} __u;
        __u.__f = __f;
        return __u.__i;
    }

#endif