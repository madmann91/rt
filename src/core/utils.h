#ifndef CORE_UTILS_H
#define CORE_UTILS_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "core/config.h"

#define IGNORE(x) do { (void)x; } while (0)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define GEN_SWAP(name, T) \
    static inline void swap_##name(T* left, T* right) { \
        T tmp = *left; \
        *left = *right; \
        *right = tmp; \
    }

static inline void die(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    abort();
}

static inline void* xmalloc(size_t size) {
    if (size == 0)
        return NULL;
    void* ptr = malloc(size);
    if (!ptr)
        die("not enough memory");
    return ptr;
}

static inline void* xrealloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    ptr = realloc(ptr, size);
    if (!ptr)
        die("not enough memory");
    return ptr;
}

static inline void* xcalloc(size_t len, size_t size) {
    if (size == 0 || len == 0)
        return NULL;
    void* ptr = calloc(len, size);
    if (!ptr)
        die("not enough memory");
    return ptr;
}

static inline real_t min_real(real_t x, real_t y) {
    // Ensures that the result is not a NaN if `y` is not a NaN
    return x < y ? x : y;
}

static inline real_t max_real(real_t x, real_t y) {
    // See `min_real()`
    return x > y ? x : y;
}

static inline real_t clamp_real(real_t x, real_t min, real_t max) {
    return min_real(max_real(x, min), max);
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

static inline real_t lerp3_real(real_t x, real_t y, real_t z, real_t u, real_t v) {
    return fast_mul_add(y, u, fast_mul_add(z, v, x * (((real_t)1) - u - v)));
}

static inline real_t lerp4_real(real_t x, real_t y, real_t z, real_t w, real_t u, real_t v) {
    return fast_mul_add(
        fast_mul_add(x, ((real_t)1) - u, y * u), ((real_t)1) - v,
        fast_mul_add(z, ((real_t)1) - u, w * u) * v);
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

static inline size_t round_up(size_t i, size_t j) {
    return i / j + (i % j ? 1 : 0);
}

static inline char* copy_str_n(const char* p, size_t n) {
    char* q = xmalloc(n + 1);
    memcpy(q, p, n);
    q[n] = 0;
    return q;
}

static inline char* copy_str(const char* p) {
    return copy_str_n(p, strlen(p));
}

#endif
