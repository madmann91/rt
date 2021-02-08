#ifndef CORE_THREAD_POOL_H
#define CORE_THREAD_POOL_H

#include <stddef.h>

struct work_item;

typedef void (*work_fn_t)(struct work_item*, size_t);

struct work_item {
    work_fn_t work_fn;
    struct work_item* next;
};

struct range {
    size_t begin, end;
};

struct parallel_task_1d {
    struct work_item work_item;
    struct range range;
};

struct parallel_task_2d {
    struct work_item work_item;
    struct range range[2];
};

/* This function tries to detect the number of threads available on the system.
 * It always returns a value greater than 0, even if detection fails.
 */
size_t detect_system_thread_count(void);

// Creates a new thread pool with an empty queue.
struct thread_pool* new_thread_pool(size_t thread_count);
// Destroys the thread pool, and terminates the worker threads, without waiting for completion.
void free_thread_pool(struct thread_pool* thread_pool);

// Returns the number of worker threads contained in the given pool.
size_t get_thread_count(const struct thread_pool* thread_pool);

// Enqueues several work items in order on a thread pool, using locks to prevent data races.
void submit_work(struct thread_pool* thread_pool, struct work_item* first, struct work_item* last);

/* Waits for the given number of enqueued work items to terminate, or all of them if `count == 0`.
 * Returns the executed work items for re-use.
 */
struct work_item* wait_for_completion(struct thread_pool* thread_pool, size_t count);

static inline size_t compute_chunk_size(size_t elem_count, size_t chunk_count) {
    return elem_count / chunk_count + (elem_count % chunk_count ? 1 : 0);
}

static inline size_t compute_chunk_begin(size_t chunk_size, size_t chunk_index) {
    return chunk_size * chunk_index;
}

static inline size_t compute_chunk_end(size_t chunk_size, size_t chunk_index, size_t count) {
    size_t chunk_end = chunk_size * (chunk_index + 1);
    return chunk_end < count ? chunk_end : count;
}

// Runs the given computation in parallel on the given thread pool.
void parallel_for_1d(
    struct thread_pool* thread_pool,
    void (*compute)(struct parallel_task_1d*, size_t),
    struct parallel_task_1d* init, size_t task_size,
    const struct range* range);

// Same, but in 2D.
void parallel_for_2d(
    struct thread_pool* thread_pool,
    void (*compute)(struct parallel_task_2d*, size_t),
    struct parallel_task_2d* init, size_t task_size,
    const struct range* range);

#endif
