#include <string.h>
#include <assert.h>

#include "core/parallel.h"

static inline struct parallel_task* task_at(
    struct parallel_task* tasks, size_t task_size, size_t index)
{
    return (struct parallel_task*)(((char*)tasks) + task_size * index);
}

static inline void init_tasks(
    struct parallel_task* tasks,
    struct parallel_task* init,
    void (*compute)(struct parallel_task*),
    size_t task_size, size_t task_count)
{
    init->work_item.work_fn = (work_fn_t)compute; 
    init->work_item.next    = NULL;
    for (size_t i = 0; i < task_count; ++i) {
        struct parallel_task* task = task_at(tasks, task_size, i);
        memcpy(task, init, task_size);
        if (i != task_count - 1)
            task->work_item.next = &task_at(tasks, task_size, i + 1)->work_item;
    }
}

static inline size_t derive_chunk_count(size_t elem_count, size_t task_count) {
    if (task_count * 2 >= elem_count)
        return 1;
    return elem_count / (task_count * 2);
}

struct parallel_task* parallel_for(
    struct thread_pool* thread_pool,
    struct mem_pool** mem_pool,
    void (*compute)(struct parallel_task*),
    struct parallel_task* init,
    size_t task_size,
    size_t begin[3], size_t end[3])
{
    size_t thread_count = get_thread_count(thread_pool);
    size_t task_count = thread_count * 2;
    struct parallel_task* tasks = alloc_from_pool(mem_pool, task_size * task_count);
    init_tasks(tasks, init, compute, task_size, task_count);

    struct parallel_task* current_task  = tasks;
    struct parallel_task* first_task    = tasks;
    struct parallel_task* previous_task = NULL;

    size_t chunk_count[3], chunk_size[3];
    for (int i = 0; i < 3; ++i) {
        chunk_count[i] = derive_chunk_count(end[i] - begin[i], task_count);
        chunk_size[i] = (end[i] - begin[i]) / chunk_count[i];
    }
    for (size_t i = begin[2]; i < end[2]; i += chunk_size[2]) {
        size_t next_i = i + chunk_size[2] > end[2] ? end[2] : i + chunk_size[2];
        for (size_t j = begin[1]; j < end[1]; j += chunk_size[1]) {
            size_t next_j = j + chunk_size[1] > end[1] ? end[1] : j + chunk_size[1];
            for (size_t k = begin[0]; k < end[0]; k += chunk_size[0]) {
                size_t next_k = k + chunk_size[0] > end[0] ? end[0] : k + chunk_size[0];
                assert(current_task);
                current_task->begin[0] = k;
                current_task->begin[1] = j;
                current_task->begin[2] = i;
                current_task->end[0] = next_k;
                current_task->end[1] = next_j;
                current_task->end[2] = next_i;
                previous_task = current_task;
                current_task = (struct parallel_task*)current_task->work_item.next;
                if (!current_task) {
                    submit_work(thread_pool, &first_task->work_item, &previous_task->work_item);
                    first_task = current_task =
                        (struct parallel_task*)wait_for_completion(thread_pool, thread_count);
                    previous_task = NULL;
                }
            }
        }
    }
    if (previous_task)
        submit_work(thread_pool, &first_task->work_item, &previous_task->work_item);
    wait_for_completion(thread_pool, 0);
    for (size_t i = 0; i < task_count - 1; ++i)
        task_at(tasks, task_size, i)->work_item.next = &task_at(tasks, task_size, i)->work_item;
    task_at(tasks, task_size, task_count - 1)->work_item.next = NULL;
    return tasks;
}
