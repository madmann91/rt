#ifndef CORE_RADIX_SORT_H
#define CORE_RADIX_SORT_H

#include <stdint.h>

#include "core/thread_pool.h"

/* Performs a radix sort over the given array.
 * This function requires a copy of the key and value buffers, as it does not operate in place.
 * The sorted array is available as the pair `(src_keys, src_values)`.
 * Supported key types are `uint8_t`, `uint16_t`, `uint32_t`, and `uint64_t`.
 * The parameter `key_size` should be set accordingly
 * (using `sizeof(T)` where `T` is the chosen key type in the above list).
 */
void radix_sort(
    struct thread_pool* thread_pool,
    void** src_keys, size_t** src_values,
    void** dst_keys, size_t** dst_values,
    size_t key_size, size_t count, unsigned bit_count);

#endif
