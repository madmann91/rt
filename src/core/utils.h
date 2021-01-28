#ifndef CORE_UTILS_H
#define CORE_UTILS_H

#include <string.h>
#include <time.h>
#include <tgmath.h>

#include "core/config.h"

static inline real_t min_real(real_t x, real_t y) {
    // Ensures that the result is not a NaN if `y` is not a NaN
    return x < y ? x : y;
}

static inline real_t max_real(real_t x, real_t y) {
    // See `min_real()`
    return x > y ? x : y;
}

static inline real_t fast_mul_add(real_t x, real_t y, real_t z) {
#ifdef FAST_REAL_FMA
    // GCC has FP_FAST_FMA defined when a fast version of `fma()` is available
    return fma(x, y, z);
#else
    // clang supports the `STDC FP_CONTRACT` pragma
    // Other compilers may ignore it
#pragma STDC FP_CONTRACT ON
    return x * y + z;
#endif
}

static inline real_t safe_inverse(real_t x) {
    return 1.0 / (fabs(x) <= REAL_EPSILON ? copysign(REAL_EPSILON, x) : x);
}

static inline bits_t float_to_bits(real_t x) {
    bits_t u;
    memcpy(&u, &x, sizeof(real_t));
    return u;
}

static inline real_t bits_to_float(bits_t u) {
    real_t x;
    memcpy(&x, &u, sizeof(real_t));
    return x;
}

static inline real_t add_ulp_magnitude(real_t x, unsigned ulps) {
    return isfinite(x) ? bits_to_float(float_to_bits(x) + ulps) : x;
}

static inline double elapsed_seconds(const struct timespec* t_start, const struct timespec* t_end) {
    return
        (double)(t_end->tv_sec - t_start->tv_sec) +
        (double)(t_end->tv_nsec - t_start->tv_nsec) * 1.0e-9;
}

#endif
