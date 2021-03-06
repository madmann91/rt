#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <threads.h>

#include "core/thread_pool.h"
#include "core/config.h"
#include "core/utils.h"

/* Fallback thread count for a thread pool when
 * the number of threads cannot be determined.
 */
#define DEFAULT_THREAD_COUNT 2

struct work_queue {
    struct work_item* first_item; // Where the worker threads take work items from
    struct work_item* last_item;  // Where the client's work items are enqueued
    struct work_item* done_items; // Where finished work items are placed
    size_t done_count;            // The number of items that are finished
    size_t done_target;           // The number of items that are required before the next synchronization
    size_t worked_on;             // The number of items being worked on
    cnd_t avail_cond, done_cond;
    mtx_t mutex;
};

struct thread_data {
    struct thread_pool* thread_pool;
    struct work_queue* queue;
    size_t thread_id;
};

struct thread_pool {
    thrd_t* threads;
    size_t thread_count;
    bool should_stop;
    struct work_queue queue;
    struct thread_data* thread_data;
};

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <unistd.h>
static inline long get_system_thread_count(void) {
    return sysconf(_SC_NPROCESSORS_ONLN); 
}
#endif

size_t detect_system_thread_count(void) {
    const char* nproc_str;
    long thread_count;
    if ((nproc_str = getenv("NPROC")) &&
        (thread_count = strtol(nproc_str, NULL, 10)) > 0)
        return thread_count;
    if ((thread_count = get_system_thread_count()) > 0)
        return thread_count;
    return DEFAULT_THREAD_COUNT;
}

static inline bool waiting_condition(const struct work_queue* queue) {
    return
        (queue->worked_on > 0 || queue->first_item) &&
        (queue->done_target == 0 || queue->done_count < queue->done_target);
}

static int thread_pool_worker(void* data) {
    struct thread_data* thread_data = data;
    struct work_queue* queue = thread_data->queue;
    struct thread_pool* thread_pool = thread_data->thread_pool;
    size_t thread_id = thread_data->thread_id;
    while (true) {
        mtx_lock(&queue->mutex);
        while (!queue->first_item) {
            if (thread_pool->should_stop || cnd_wait(&queue->avail_cond, &queue->mutex) != thrd_success)
                goto end;
        }
        struct work_item* item = queue->first_item;
        queue->first_item = item->next;
        if (!queue->first_item) {
            assert(queue->last_item == item);
            queue->last_item = NULL;
        }
        queue->worked_on++;
        mtx_unlock(&queue->mutex);

        item->work_fn(item, thread_id);

        mtx_lock(&queue->mutex);
        item->next = queue->done_items;
        queue->done_items = item;
        queue->worked_on--;
        queue->done_count++;
        if (!waiting_condition(queue))
            cnd_signal(&queue->done_cond);
        mtx_unlock(&queue->mutex);
    }
end:
    mtx_unlock(&queue->mutex);
    return 0;
}

static inline bool init_work_queue(struct work_queue* queue) {
    if (cnd_init(&queue->avail_cond) != thrd_success)
        return false;        
    if (cnd_init(&queue->done_cond) != thrd_success)
        goto cleanup_cond;
    if (mtx_init(&queue->mutex, mtx_plain) != thrd_success)
        goto cleanup_mutex;
    queue->first_item = NULL;
    queue->last_item = NULL;
    queue->done_items = NULL;
    queue->worked_on = 0;
    queue->done_count = 0;
    queue->done_target = 0;
    return true;
cleanup_mutex:
    cnd_destroy(&queue->done_cond);
cleanup_cond:
    cnd_destroy(&queue->avail_cond);
    return false;
}

static inline void free_work_queue(struct work_queue* queue) {
    mtx_destroy(&queue->mutex);
    cnd_destroy(&queue->avail_cond);
    cnd_destroy(&queue->done_cond);
}

static inline void terminate_threads(struct thread_pool* thread_pool) {
    mtx_lock(&thread_pool->queue.mutex);
    thread_pool->should_stop = true;
    cnd_broadcast(&thread_pool->queue.avail_cond);
    mtx_unlock(&thread_pool->queue.mutex);
    for (size_t i = 0, n = thread_pool->thread_count; i < n; ++i)
        thrd_join(thread_pool->threads[i], NULL);
}

struct thread_pool* new_thread_pool(size_t thread_count) {
    assert(thread_count > 0);
    struct thread_pool* thread_pool = xmalloc(sizeof(struct thread_pool));
    if (!init_work_queue(&thread_pool->queue))
        goto cleanup_queue;
    thread_pool->thread_data = xmalloc(sizeof(struct thread_data) * thread_count);
    thread_pool->threads = xmalloc(sizeof(thrd_t) * thread_count);
    thread_pool->thread_count = thread_count;
    thread_pool->should_stop = false;
    for (size_t i = 0; i < thread_count; ++i) {
        thread_pool->thread_data[i].thread_pool = thread_pool;
        thread_pool->thread_data[i].queue = &thread_pool->queue;
        thread_pool->thread_data[i].thread_id = i;
        if (thrd_create(thread_pool->threads + i, thread_pool_worker, &thread_pool->thread_data[i]) != thrd_success) {
            thread_pool->thread_count = i;
            goto cleanup_thread;
        }
    }
    return thread_pool;
cleanup_thread:
    terminate_threads(thread_pool);
    free_work_queue(&thread_pool->queue);
    free(thread_pool->threads);
    free(thread_pool->thread_data);
cleanup_queue:
    free(thread_pool);
    return NULL;
}

void free_thread_pool(struct thread_pool* thread_pool) {
    terminate_threads(thread_pool);
    free_work_queue(&thread_pool->queue);
    free(thread_pool->threads);
    free(thread_pool->thread_data);
    free(thread_pool);
}

size_t get_thread_count(const struct thread_pool* thread_pool) {
    return thread_pool->thread_count;
}

void submit_work(struct thread_pool* thread_pool, struct work_item* first, struct work_item* last) {
#ifndef NDEBUG
    // Ensure that following the links from `first` gives `last` as the last element.
    struct work_item* prev = first;
    while (prev->next) prev = prev->next;
    assert(prev == last);
#endif
    mtx_lock(&thread_pool->queue.mutex);
    if (thread_pool->queue.last_item) {
        assert(thread_pool->queue.first_item);
        thread_pool->queue.last_item->next = first;
        thread_pool->queue.last_item = last;
    } else {
        assert(!thread_pool->queue.first_item);
        thread_pool->queue.first_item = first;
        thread_pool->queue.last_item  = last;
    }
    if (first == last)
        cnd_signal(&thread_pool->queue.avail_cond);
    else
        cnd_broadcast(&thread_pool->queue.avail_cond);
    mtx_unlock(&thread_pool->queue.mutex);
}

struct work_item* wait_for_completion(struct thread_pool* thread_pool, size_t count) {
    struct work_queue* queue = &thread_pool->queue;
    struct work_item* done_items = NULL;
    mtx_lock(&queue->mutex);
    queue->done_target = count;
    while (true) {
        if (waiting_condition(queue) &&
            cnd_wait(&queue->done_cond, &queue->mutex) == thrd_success)
            continue;
        break;
    }
    done_items = queue->done_items;
    queue->done_items = NULL;
    queue->done_count = 0;
    queue->done_target = 0;
    mtx_unlock(&queue->mutex);
    return done_items;
}

static inline struct work_item* task_at(
    struct work_item* tasks, size_t task_size, size_t index)
{
    return (struct work_item*)(((char*)tasks) + task_size * index);
}

static inline void init_parallel_tasks(
    struct work_item* tasks,
    struct work_item* init,
    void (*compute)(struct work_item*, size_t),
    size_t task_size, size_t task_count)
{
    init->work_fn = compute; 
    init->next    = NULL;
    for (size_t i = 0; i < task_count; ++i) {
        struct work_item* task = task_at(tasks, task_size, i);
        memcpy(task, init, task_size);
        if (i != task_count - 1)
            task->next = task_at(tasks, task_size, i + 1);
    }
}

static inline size_t range_end(size_t i, size_t chunk_size, size_t end) {
    return i + chunk_size > end ? end : i + chunk_size;
}

void parallel_for_1d(
    struct thread_pool* thread_pool,
    void (*compute)(struct parallel_task_1d*, size_t),
    struct parallel_task_1d* init, size_t task_size,
    const struct range* range)
{
    size_t thread_count = get_thread_count(thread_pool);
    size_t task_count = thread_count * 2;
    struct work_item* tasks = xmalloc(task_size * task_count);
    init_parallel_tasks(tasks, &init->work_item, (work_fn_t)compute, task_size, task_count);

    struct work_item* current_task  = tasks;
    struct work_item* first_task    = tasks;
    struct work_item* previous_task = NULL;

    const size_t chunk_size = compute_chunk_size(range[0].end - range[0].begin, task_count);
    for (size_t i = range[0].begin; i < range[0].end; i += chunk_size) {
        assert(current_task);
        ((struct parallel_task_1d*)current_task)->range.begin = i;
        ((struct parallel_task_1d*)current_task)->range.end   = range_end(i, chunk_size, range[0].end);
        previous_task = current_task;
        current_task = current_task->next;
        if (!current_task) {
            submit_work(thread_pool, first_task, previous_task);
            first_task = current_task = wait_for_completion(thread_pool, thread_count);
            previous_task = NULL;
        }
    }
    if (previous_task) {
        previous_task->next = NULL;
        submit_work(thread_pool, first_task, previous_task);
    }
    wait_for_completion(thread_pool, 0);
    free(tasks);
}

void parallel_for_2d(
    struct thread_pool* thread_pool,
    void (*compute)(struct parallel_task_2d*, size_t),
    struct parallel_task_2d* init, size_t task_size,
    const struct range* range)
{
    size_t thread_count = get_thread_count(thread_pool);
    size_t task_count = thread_count * 2;
    struct work_item* tasks = xmalloc(task_size * task_count);
    init_parallel_tasks(tasks, &init->work_item, (work_fn_t)compute, task_size, task_count);

    struct work_item* current_task  = tasks;
    struct work_item* first_task    = tasks;
    struct work_item* previous_task = NULL;

    const size_t chunk_size[] = {
        compute_chunk_size(range[0].end - range[0].begin, task_count),
        compute_chunk_size(range[1].end - range[1].begin, task_count)
    };
    for (size_t j = range[1].begin; j < range[1].end; j += chunk_size[1]) {
        size_t next_j = range_end(j, chunk_size[1], range[1].end);
        for (size_t i = range[0].begin; i < range[0].end; i += chunk_size[0]) {
            size_t next_i = range_end(i, chunk_size[0], range[0].end);
            assert(current_task);
            ((struct parallel_task_2d*)current_task)->range[0].begin = i;
            ((struct parallel_task_2d*)current_task)->range[0].end   = next_i;
            ((struct parallel_task_2d*)current_task)->range[1].begin = j;
            ((struct parallel_task_2d*)current_task)->range[1].end   = next_j;
            previous_task = current_task;
            current_task = current_task->next;
            if (!current_task) {
                submit_work(thread_pool, first_task, previous_task);
                first_task = current_task = wait_for_completion(thread_pool, thread_count);
                previous_task = NULL;
            }
        }
    }
    if (previous_task) {
        previous_task->next = NULL;
        submit_work(thread_pool, first_task, previous_task);
    }
    wait_for_completion(thread_pool, 0);
    free(tasks);
}
