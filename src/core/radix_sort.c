#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include "core/radix_sort.h"
#include "core/parallel.h"
#include "core/alloc.h"
#include "core/utils.h"

#define RADIX_SORT_BITS 8
#define BIN_COUNT (1 << RADIX_SORT_BITS)
#define UINT_N(bit_count) uint##bit_count##_t
#define BINNING_MASK(bit_count) \
    (((UINT_N(bit_count))-1) >> ( \
        bit_count > RADIX_SORT_BITS ? \
        bit_count - RADIX_SORT_BITS : 0))

struct binning_task {
    struct work_item work_item;
    size_t begin, end;
    void** src_keys;
    unsigned first_bit;
    size_t bins[BIN_COUNT];
};

#define BINNING(bit_count) \
    static void run_##bit_count##_bit_binning_task(struct work_item* work_item) { \
        struct binning_task* binning_task = (void*)work_item; \
        const UINT_N(bit_count)* keys = *binning_task->src_keys; \
        UINT_N(bit_count) mask = BINNING_MASK(bit_count); \
        UINT_N(bit_count) shift = binning_task->first_bit; \
        memset(binning_task->bins, 0, sizeof(size_t) * BIN_COUNT); \
        for (size_t i = binning_task->begin, n = binning_task->end; i < n; ++i) \
            binning_task->bins[(keys[i] >> shift) & mask]++; \
    }

BINNING(8)
BINNING(16)
BINNING(32)
BINNING(64)

static const work_fn_t binning_fns[] = {
    [sizeof(uint8_t )] = run_8_bit_binning_task,
    [sizeof(uint16_t)] = run_16_bit_binning_task,
    [sizeof(uint32_t)] = run_32_bit_binning_task,
    [sizeof(uint64_t)] = run_64_bit_binning_task,
};

struct prefix_sum_task {
    struct work_item work_item;
    struct binning_task* binning_tasks;
    size_t* shared_bins;
    size_t binning_task_count;
    size_t begin, end;
};

static void prefix_sum_task(struct work_item* work_item) {
    struct prefix_sum_task* prefix_sum_task = (void*)work_item;
    struct binning_task* binning_tasks = prefix_sum_task->binning_tasks;
    for (size_t i = prefix_sum_task->begin, n = prefix_sum_task->end; i < n; ++i) {
        size_t sum = 0;
        for (size_t j = 0, m = prefix_sum_task->binning_task_count; j < m; ++j) {
            size_t old_sum = sum;
            sum += binning_tasks[j].bins[i];
            binning_tasks[j].bins[i] = old_sum;
        }
        prefix_sum_task->shared_bins[i] = sum;
    }
}

struct copy_task {
    struct work_item work_item;
    const size_t* shared_bins;
    size_t** src_values;
    void** dst_keys;
    size_t** dst_values;
    struct binning_task* this_binning_task;
    size_t begin, end;
};

#define COPY(bit_count) \
    static void run_##bit_count##_bit_copy_task(struct work_item* work_item) { \
        struct copy_task* copy_task = (void*)work_item; \
        struct binning_task* this_binning_task = copy_task->this_binning_task; \
        size_t sum = 0; \
        for (size_t i = 0; i < BIN_COUNT; ++i) { \
            size_t old_sum = sum; \
            sum += copy_task->shared_bins[i]; \
            this_binning_task->bins[i] += old_sum; \
        } \
        const UINT_N(bit_count)* src_keys = *this_binning_task->src_keys; \
        const size_t* src_values = *copy_task->src_values; \
        UINT_N(bit_count)* dst_keys = *copy_task->dst_keys; \
        size_t* dst_values = *copy_task->dst_values; \
        UINT_N(bit_count) mask = BINNING_MASK(bit_count); \
        UINT_N(bit_count) shift = this_binning_task->first_bit; \
        for (size_t i = copy_task->begin, n = copy_task->end; i < n; ++i) { \
            size_t index = this_binning_task->bins[(src_keys[i] >> shift) & mask]++; \
            dst_keys[index] = src_keys[i]; \
            dst_values[index] = src_values[i]; \
        } \
    }

COPY(8)
COPY(16)
COPY(32)
COPY(64)

static const work_fn_t copy_fns[] = {
    [sizeof(uint8_t )] = run_8_bit_copy_task,
    [sizeof(uint16_t)] = run_16_bit_copy_task,
    [sizeof(uint32_t)] = run_32_bit_copy_task,
    [sizeof(uint64_t)] = run_64_bit_copy_task
};

SWAP(keys, void*)
SWAP(values, size_t*)

void radix_sort(
    struct thread_pool* thread_pool,
    void** src_keys, size_t** src_values,
    void** dst_keys, size_t** dst_values,
    size_t key_size, size_t count, unsigned bit_count)
{
    size_t thread_count = get_thread_count(thread_pool);

    struct binning_task* binning_tasks = xmalloc(sizeof(struct binning_task) * thread_count);
    struct copy_task* copy_tasks       = xmalloc(sizeof(struct copy_task) * thread_count);
    struct prefix_sum_task* sum_tasks  = xmalloc(sizeof(struct prefix_sum_task) * thread_count);
    size_t* shared_bins                = xmalloc(sizeof(size_t) * BIN_COUNT);

    assert(key_size < ARRAY_SIZE(binning_fns) && binning_fns[key_size]);
    size_t data_chunk_size = compute_chunk_size(count,     thread_count);
    size_t bin_chunk_size  = compute_chunk_size(BIN_COUNT, thread_count);
    for (size_t j = 0; j < thread_count; ++j) {
        copy_tasks[j].begin = binning_tasks[j].begin = compute_chunk_begin(data_chunk_size, j);
        copy_tasks[j].end   = binning_tasks[j].end   = compute_chunk_end(data_chunk_size, j, count);
        binning_tasks[j].work_item.work_fn = binning_fns[key_size];
        binning_tasks[j].src_keys = src_keys;
        copy_tasks[j].work_item.work_fn    = copy_fns[key_size];
        copy_tasks[j].this_binning_task = &binning_tasks[j];
        copy_tasks[j].shared_bins = shared_bins;
        copy_tasks[j].src_values = src_values;
        copy_tasks[j].dst_values = dst_values;
        copy_tasks[j].dst_keys = dst_keys;

        sum_tasks[j].begin = compute_chunk_begin(bin_chunk_size, j);
        sum_tasks[j].end   = compute_chunk_end(bin_chunk_size, j, BIN_COUNT);
        sum_tasks[j].shared_bins = shared_bins;
        sum_tasks[j].binning_tasks = binning_tasks;
        sum_tasks[j].binning_task_count = thread_count;
        sum_tasks[j].work_item.work_fn = prefix_sum_task;
    }

    for (unsigned i = 0; i < bit_count; i += RADIX_SORT_BITS) {
        // Perform binning over the input array
        for (size_t j = 0; j < thread_count; ++j) {
            binning_tasks[j].work_item.next = &binning_tasks[j + 1].work_item;
            binning_tasks[j].first_bit = i;
        }
        binning_tasks[thread_count - 1].work_item.next = NULL;
        submit_work(thread_pool, &binning_tasks[0].work_item, &binning_tasks[thread_count - 1].work_item);

        // Reset the task data for summing and copying
        for (size_t j = 1; j < thread_count; ++j) {
            sum_tasks[j - 1].work_item.next  = &sum_tasks[j].work_item;
            copy_tasks[j - 1].work_item.next = &copy_tasks[j].work_item;
        }
        copy_tasks[thread_count - 1].work_item.next = NULL;
        sum_tasks[thread_count - 1].work_item.next  = NULL;

        // Do a prefix sum over the bins, in parallel
        wait_for_completion(thread_pool, 0);
        submit_work(thread_pool, &sum_tasks[0].work_item, &sum_tasks[thread_count - 1].work_item);

        // Place the sorted data for that bit range in the destination arrays
        wait_for_completion(thread_pool, 0);
        submit_work(thread_pool, &copy_tasks[0].work_item, &copy_tasks[thread_count - 1].work_item);

        wait_for_completion(thread_pool, 0);
        swap_keys(src_keys, dst_keys);
        swap_values(src_values, dst_values);
    }

    free(binning_tasks);
    free(sum_tasks);
    free(copy_tasks);
    free(shared_bins);
}
