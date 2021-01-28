#ifndef CORE_THREAD_POOL_H
#define CORE_THREAD_POOL_H

#include <stddef.h>

struct work_item;

typedef void (*work_fn_t)(struct work_item*);

struct work_item {
    work_fn_t work_fn;
    struct work_item* next;
};

/* This function tries to detect the number of threads available on the system.
 * It always returns a value greater than 0, even if detection fails.
 */
size_t detect_system_thread_count(void);

/* Creates a new thread pool with an empty queue.  */
struct thread_pool* new_thread_pool(size_t thread_count);
/* Destroys the thread pool, and terminates the worker threads, without waiting for completion. */
void free_thread_pool(struct thread_pool* thread_pool);

/* Enqueues several work items in order on a thread pool, using locks to prevent data races. */
void submit_work(struct thread_pool* thread_pool, struct work_item* first, struct work_item* last);

/* Waits for the given number of enqueued work items to terminate, or all of them if `count == 0`.
 * Returns the executed work items for re-use.
 */
struct work_item* wait_for_completion(struct thread_pool* thread_pool, size_t count);

#endif
