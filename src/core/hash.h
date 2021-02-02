#ifndef CORE_HASH_H
#define CORE_HASH_H

#include <stdint.h>
#include <stddef.h>

#define FNV_OFFSET UINT32_C(0x811C9DC5) // Initial value for an empty hash
#define FNV_PRIME  UINT32_C(0x01000193)

#define hash_uint(h, x) _Generic((x), \
    uint8_t: hash_uint8, \
    uint16_t: hash_uint16, \
    uint32_t: hash_uint32, \
    uint64_t: hash_uint64) \
    (h, x)

static inline uint32_t hash_init(void) {
    return FNV_OFFSET;
}

static inline uint32_t hash_uint8(uint32_t h, uint8_t u) {
    return (h ^ u) * FNV_PRIME;
}

static inline uint32_t hash_uint16(uint32_t h, uint16_t u) {
    return hash_uint8(hash_uint8(h, u), u >> 8);
}

static inline uint32_t hash_uint32(uint32_t h, uint32_t u) {
    return hash_uint16(hash_uint16(h, u), u >> 16);
}

static inline uint32_t hash_uint64(uint32_t h, uint64_t u) {
    return hash_uint32(hash_uint32(h, u), u >> 32);
}

static inline uint32_t hash_bytes(uint32_t h, const void* data, size_t size) {
    for (size_t i = 0; i < size; ++i)
        h = hash_uint8(h, ((const uint8_t*)data)[i]);
    return h;
}

static inline uint32_t hash_ptr(uint32_t h, const void* ptr) {
    return hash_uint(h, (uintptr_t)ptr);
}

static inline uint32_t hash_str(uint32_t h, const char* str) {
    for (; *str; str++)
        h = hash_uint(h, *(unsigned char*)str);
    return h;
}

#endif
