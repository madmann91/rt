#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>

#include "core/radix_sort.h"
#include "core/hash.h"
#include "core/utils.h"

static bool is_sorted(const uint32_t* keys, size_t count) {
    for (size_t i = 1; i < count; ++i) {
        if (keys[i - 1] > keys[i])
            return false;
    }
    return true;
}

int main() {
    size_t count = 10000000, iter_count = 100;
    uint32_t* src_keys  = xmalloc(sizeof(uint32_t) * count);
    uint32_t* dst_keys  = xmalloc(sizeof(uint32_t) * count);
    size_t* src_values  = xmalloc(sizeof(size_t) * count);
    size_t* dst_values  = xmalloc(sizeof(size_t) * count);
    int status = EXIT_SUCCESS;

    size_t thread_count = detect_system_thread_count();
    struct thread_pool* thread_pool = new_thread_pool(thread_count);

    struct timespec t_start;
    timespec_get(&t_start, TIME_UTC);
    for (size_t iter = 0; iter < iter_count; ++iter) {
        for (size_t i = 0; i < count; ++i) {
            src_keys[i] = hash_uint(hash_init(), i);
            src_values[i] = i;
        }
        void* src_keys_p = src_keys;
        void* dst_keys_p = dst_keys;
        radix_sort(
            thread_pool,
            &src_keys_p, &src_values,
            &dst_keys_p, &dst_values,
            sizeof(uint32_t),
            count, sizeof(uint32_t) * CHAR_BIT);
        src_keys = src_keys_p;
        dst_keys = dst_keys_p;
    }
    struct timespec t_end;
    timespec_get(&t_end, TIME_UTC);

    free_thread_pool(thread_pool);

    printf("Sorting took %g seconds\n", elapsed_seconds(&t_start, &t_end) / iter_count);
    if (!is_sorted(src_keys, count)) {
        fprintf(stderr, "Test failed: The elements are not sorted\n");
        if (count <= 100) {
            for (size_t i = 0; i < count; ++i)
                fprintf(stderr, "%"PRIu32" ", src_keys[i]);
            fprintf(stderr, "\n");
        }
        status = EXIT_FAILURE;
    }

    free(src_keys);
    free(dst_keys);
    free(src_values);
    free(dst_values);
    return status;
}
