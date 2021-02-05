#ifndef CORE_MORTON_H
#define CORE_MORTON_H

#include <stdint.h>
#include <stddef.h>

#ifdef USE_64_BIT_MORTON_CODES
#define MORTON_LOG_BITS 6
typedef uint64_t morton_t;
#else
#define MORTON_LOG_BITS 5
typedef uint32_t morton_t;
#endif
#define MORTON_GRID_DIM (((size_t)1) << (sizeof(morton_t) * CHAR_BIT / 3))

// Split the bit pattern of `x` such that each bit is separated from another bit two zeros.
static inline morton_t morton_split(morton_t x) {
    morton_t mask = -1;
    for (size_t i = MORTON_LOG_BITS, n = 1 << MORTON_LOG_BITS; i > 0; --i, n >>= 1) {
        mask = (mask | (mask << n)) & ~(mask << (n / 2));
        x = (x | (x << n)) & mask;
    }
    return x;
}

static inline morton_t morton_encode(morton_t x, morton_t y, morton_t z) {
    return morton_split(x) | (morton_split(y) << 1) | (morton_split(z) << 2);
}

#endif
