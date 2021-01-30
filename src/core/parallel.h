#ifndef CORE_PARALLEL_H
#define CORE_PARALLEL_H

#include <stddef.h>

#include "core/thread_pool.h"
#include "core/mem_pool.h"

struct parallel_task {
    struct work_item work_item;
    size_t begin[3], end[3];
};

/* Runs the given computation in parallel on the given thread pool.
 * Uses the memory pool to allocate tasks.
 * Returns the list of tasks, allocated from the memory pool.
 */
struct parallel_task* parallel_for(
    struct thread_pool* thread_pool,
    struct mem_pool** mem_pool,
    void (*compute)(struct parallel_task*),
    struct parallel_task* init,
    size_t task_size,
    size_t begin[3], size_t end[3]);

#endif
