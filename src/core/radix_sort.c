#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include "core/radix_sort.h"
#include "core/alloc.h"
#include "core/utils.h"

#define RADIX_SORT_BITS 8
#define BIN_COUNT (1 << RADIX_SORT_BITS)
#define BINNING_MASK(T) \
    (((T)-1) >> ( \
        sizeof(T) * CHAR_BIT > RADIX_SORT_BITS ? \
        sizeof(T) * CHAR_BIT - RADIX_SORT_BITS : 0))

struct binning_task {
    struct work_item work_item;
    size_t begin, end;
    void** src_keys;
    unsigned first_bit;
    size_t bins[BIN_COUNT];
};

#define BINNING(name, T) \
    static void binning_##name(struct work_item* work_item) { \
        struct binning_task* binning_task = (void*)work_item; \
        const T* keys = *binning_task->src_keys; \
        T mask = BINNING_MASK(T); \
        T shift = binning_task->first_bit; \
        memset(binning_task->bins, 0, sizeof(size_t) * BIN_COUNT); \
        for (size_t i = binning_task->begin, n = binning_task->end; i < n; ++i) \
            binning_task->bins[(keys[i] >> shift) & mask]++; \
    }

BINNING(8_bit,  uint8_t)
BINNING(16_bit, uint16_t)
BINNING(32_bit, uint32_t)
BINNING(64_bit, uint64_t)

static const work_fn_t binning_fns[] = {
    [sizeof(uint8_t )] = binning_8_bit,
    [sizeof(uint16_t)] = binning_16_bit,
    [sizeof(uint32_t)] = binning_32_bit,
    [sizeof(uint64_t)] = binning_64_bit
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

#define COPY(name, T) \
    static void copy_##name(struct work_item* work_item) { \
        struct copy_task* copy_task = (void*)work_item; \
        struct binning_task* this_binning_task = copy_task->this_binning_task; \
        size_t sum = 0; \
        for (size_t i = 0; i < BIN_COUNT; ++i) { \
            size_t old_sum = sum; \
            sum += copy_task->shared_bins[i]; \
            this_binning_task->bins[i] += old_sum; \
        } \
        const T* src_keys = *this_binning_task->src_keys; \
        const size_t* src_values = *copy_task->src_values; \
        T* dst_keys = *copy_task->dst_keys; \
        size_t* dst_values = *copy_task->dst_values; \
        T mask = BINNING_MASK(T); \
        T shift = this_binning_task->first_bit; \
        for (size_t i = copy_task->begin, n = copy_task->end; i < n; ++i) { \
            size_t index = this_binning_task->bins[(src_keys[i] >> shift) & mask]++; \
            dst_keys[index] = src_keys[i]; \
            dst_values[index] = src_values[i]; \
        } \
    }

COPY(8_bit,  uint8_t)
COPY(16_bit, uint16_t)
COPY(32_bit, uint32_t)
COPY(64_bit, uint64_t)

static const work_fn_t copy_fns[] = {
    [sizeof(uint8_t )] = copy_8_bit,
    [sizeof(uint16_t)] = copy_16_bit,
    [sizeof(uint32_t)] = copy_32_bit,
    [sizeof(uint64_t)] = copy_64_bit
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
    size_t data_chunk_size = count / thread_count;
    size_t bin_chunk_size  = BIN_COUNT / thread_count;
    for (size_t j = 0; j < thread_count; ++j) {
        copy_tasks[j].begin = binning_tasks[j].begin = j * data_chunk_size;
        copy_tasks[j].end   = binning_tasks[j].end   = j == thread_count - 1 ? count : (j + 1) * data_chunk_size;
        binning_tasks[j].work_item.work_fn = binning_fns[key_size];
        binning_tasks[j].src_keys = src_keys;
        copy_tasks[j].work_item.work_fn    = copy_fns[key_size];
        copy_tasks[j].this_binning_task = &binning_tasks[j];
        copy_tasks[j].shared_bins = shared_bins;
        copy_tasks[j].src_values = src_values;
        copy_tasks[j].dst_values = dst_values;
        copy_tasks[j].dst_keys = dst_keys;

        sum_tasks[j].begin = j * bin_chunk_size;
        sum_tasks[j].end   = j == thread_count - 1 ? BIN_COUNT : (j + 1) * bin_chunk_size;
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
