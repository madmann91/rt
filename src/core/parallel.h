#ifndef CORE_PARALLEL_H
#define CORE_PARALLEL_H

#include <stddef.h>

#include "core/thread_pool.h"

struct parallel_task {
    struct work_item work_item;
    size_t begin[3], end[3];
};

/* Runs the given computation in parallel on the given thread pool. */
void parallel_for(
    struct thread_pool* thread_pool,
    void (*compute)(struct parallel_task*),
    struct parallel_task* init,
    size_t task_size,
    size_t begin[3], size_t end[3]);

/* Runs the given reduction in parallel on the given thread pool.
 * The parameter `result_and_init` contains the initial value of the reduction,
 * and is contains the reduction result upon exit.
 */
void reduce(
    struct thread_pool* thread_pool,
    void (*compute)(struct parallel_task*),
    void (*merge)(struct parallel_task*, const struct parallel_task*),
    struct parallel_task* result_and_init,
    size_t task_size,
    size_t begin[3], size_t end[3]);

#endif
