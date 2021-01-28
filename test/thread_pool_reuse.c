#include <stdlib.h>
#include <stdio.h>

#include "core/thread_pool.h"

struct add_job {
    struct work_item work_item;
    int* a;
};

static void add(struct work_item* work_item) {
    struct add_job* job = (void*)work_item;
    (*job->a)++;
}

int main() {
    int status = EXIT_SUCCESS;
    const size_t N = 100000;
    struct thread_pool* pool = new_thread_pool(detect_system_thread_count());
    for (size_t i = 0; i < N; ++i) {
        int a = 0;
        struct add_job job = { { add, NULL }, &a };
        submit_work(pool, &job.work_item, &job.work_item);
        wait_for_completion(pool, 0);
        a = 0;
        submit_work(pool, &job.work_item, &job.work_item);
        wait_for_completion(pool, 0);
        if (a != 1) {
            printf("\nTest failed after %zu iteration(s)", i);
            status = EXIT_FAILURE;
            break;
        }
        if (i % 100 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    free_thread_pool(pool);
    printf("\n");
    return status;
}
