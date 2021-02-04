#include <stdlib.h>
#include <stdatomic.h>
#include <assert.h>

#include "bvh/bvh.h"
#include "core/parallel.h"
#include "core/alloc.h"
#include "core/utils.h"

/* This algorithm collapses leaves according to the SAH. It is based on a bottom-up
 * traversal, which is itself inspired from T. Karras's paper: "Maximizing Parallelism in
 * the Construction of BVHs, Octrees, and k-d Trees".
 */

struct init_task {
    struct parallel_task task;
    const struct bvh_node* nodes;
    size_t* node_counts;
    size_t* parents;
    atomic_int* flags;
};

static void run_init_task(struct parallel_task* task) {
    struct init_task* init_task = (void*)task;
    for (size_t i = task->begin[0], n = task->end[0]; i < n; ++i) {
        const struct bvh_node* node = &init_task->nodes[i];
        init_task->node_counts[i] = 1;
        atomic_store_explicit(&init_task->flags[i], 0, memory_order_relaxed);
        if (node->primitive_count == 0) {
            init_task->parents[node->first_child_or_primitive + 0] = i;
            init_task->parents[node->first_child_or_primitive + 1] = i;
        }
    }
}

struct collapse_task {
    struct parallel_task task;
    const struct bvh_node* nodes;
    size_t* node_counts;
    real_t traversal_cost;
    size_t* primitive_counts;
    size_t* parents;
    atomic_int* flags;
};

static void run_collapse_task(struct parallel_task* task) {
    struct collapse_task* collapse_task = (void*)task;
    for (size_t i = task->begin[0], n = task->end[0]; i < n; ++i) {
        const struct bvh_node* node = &collapse_task->nodes[i];
        if (node->primitive_count == 0)
            continue;

        collapse_task->primitive_counts[i] = node->primitive_count;

        // Walk up the parents of this node towards the root
        size_t j = collapse_task->parents[i];
        while (j != SIZE_MAX) {
            // Terminate this path if the two children haven't been processed
            if (atomic_fetch_add_explicit(&collapse_task->flags[j], 1, memory_order_relaxed) == 0)
                break;
            const struct bvh_node* node = &collapse_task->nodes[j];
            assert(node->primitive_count == 0);

            size_t first_child = node->first_child_or_primitive;
            size_t left_count  = collapse_task->primitive_counts[first_child + 0];
            size_t right_count = collapse_task->primitive_counts[first_child + 0];
            // Both children must be leaves in order to collapse this node
            if (left_count == 0 || right_count == 0)
                break;

            const struct bvh_node* left  = &collapse_task->nodes[first_child + 0];
            const struct bvh_node* right = &collapse_task->nodes[first_child + 1];
            size_t total_count = left_count + right_count;
            float collapse_cost =
                half_bbox_area(get_bvh_node_bbox(node)) * (total_count - collapse_task->traversal_cost);
            float cost =
                half_bbox_area(get_bvh_node_bbox(left))  * left_count +
                half_bbox_area(get_bvh_node_bbox(right)) * right_count;
            if (collapse_cost < cost) {
                collapse_task->primitive_counts[j] = total_count;
                collapse_task->primitive_counts[first_child + 0] = 0;
                collapse_task->primitive_counts[first_child + 1] = 0;
                collapse_task->node_counts[first_child + 0] = 0;
                collapse_task->node_counts[first_child + 1] = 0;
            }

            j = collapse_task->parents[j];
        }
    }
}

struct counting_task {
    struct work_item work_item;
    size_t begin, end;
    const size_t* node_counts;
    const size_t* primitive_counts;
    size_t primitive_count;
    size_t node_count;
};

static void run_counting_task(struct work_item* work_item) {
    struct counting_task* counting_task = (void*)work_item;
    counting_task->primitive_count = 0;
    counting_task->node_count = 0;
    for (size_t i = counting_task->begin, n = counting_task->end; i < n; ++i) {
         counting_task->primitive_count += counting_task->primitive_counts[i];
         counting_task->node_count += counting_task->node_counts[i];
    }
}

struct rewrite_task {
    struct work_item work_item;
    size_t begin, end;
    struct bvh_node* nodes;
    const size_t* keep_nodes;
    const size_t* primitive_counts;
    const size_t* src_primitive_indices;
    size_t* dst_primitive_indices;
    size_t first_node;
    size_t first_index;
};

static void run_rewrite_task(struct work_item* work_item) {
    struct rewrite_task* rewrite_task = (void*)work_item;
    for (size_t i = rewrite_task->begin, n = rewrite_task->end; i < n; ++i) {
        if (!rewrite_task->keep_nodes[i])
            continue;
        if (rewrite_task->primitive_counts[i] != 0)
    }
}

SWAP(primitive_indices, size_t*)

void collapse_leaves(struct thread_pool* thread_pool, struct bvh* bvh, real_t traversal_cost) {
    size_t* parents   = xmalloc(sizeof(size_t) * bvh->node_count);
    atomic_int* flags = xmalloc(sizeof(atomic_int) * bvh->node_count);
    bool* keep_nodes  = xmalloc(sizeof(bool) * bvh->node_count);

    // Initialize parent indices and flags
    parallel_for(
        thread_pool,
        run_init_task,
        (struct parallel_task*)&(struct init_task) {
            .nodes      = bvh->nodes,
            .parents    = parents,
            .flags      = flags,
            .keep_nodes = keep_nodes
        },
        sizeof(struct init_task),
        (size_t[3]) { 0 },
        (size_t[3]) { bvh->node_count, 1, 1 });
    parents[0] = SIZE_MAX;

    // Traverse the BVH from bottom to top, collapsing leaves on the way
    size_t* primitive_counts = xcalloc(bvh->node_count, sizeof(size_t));
    parallel_for(
        thread_pool,
        run_collapse_task,
        (struct parallel_task*)&(struct collapse_task) {
            .primitive_counts = primitive_counts,
            .traversal_cost   = traversal_cost,
            .nodes            = bvh->nodes,
            .parents          = parents,
            .flags            = flags,
            .keep_nodes       = keep_nodes
        },
        sizeof(struct collapse_task),
        (size_t[3]) { 0 },
        (size_t[3]) { bvh->node_count, 1, 1 });

    // Perform a sum of the primitives contained in each chunk of the BVH.
    // Since leaves will most likely be in small parts of the BVH, it is
    // important to have enough tasks to process the array of nodes to
    // balance the workload efficiently.
    size_t task_count   = get_thread_count(thread_pool) * 4;
    size_t chunk_size   = compute_chunk_size(bvh->node_count, task_count);
    struct counting_task* counting_tasks = xmalloc(sizeof(struct counting_task) * task_count);
    struct rewrite_task*  rewrite_tasks = xmalloc(sizeof(struct rewrite_task) * task_count);
    for (size_t i = 0; i < task_count; ++i) {
        counting_tasks[i].work_item.work_fn = run_counting_task;
        counting_tasks[i].work_item.next    = &counting_tasks[i + 1].work_item;
        counting_tasks[i].primitive_counts  = primitive_counts;
        counting_tasks[i].keep_nodes        = keep_nodes;
        rewrite_tasks[i].begin = counting_tasks[i].begin = compute_chunk_begin(chunk_size, i, bvh->node_count);
        rewrite_tasks[i].end   = counting_tasks[i].end   = compute_chunk_end(chunk_size, i, bvh->node_count);
    }
    submit_work(thread_pool, &counting_tasks[0].work_item, &counting_tasks[task_count - 1].work_item);
    wait_for_completion(thread_pool, 0);

    // Now rewrite the primitive indices based on the previously computed sums
    size_t first_index = 0, first_node = 0;
    for (size_t i = 0; i < task_count; ++i) {
        rewrite_tasks[i].work_item.work_fn = run_rewrite_task;
        rewrite_tasks[i].work_item.next    = &rewrite_tasks[i + 1].work_item;
        rewrite_tasks[i].primitive_counts  = primitive_counts;
        rewrite_tasks[i].keep_nodes        = keep_nodes;
        rewrite_tasks[i].first_index       = first_index;
        rewrite_tasks[i].first_node        = first_node;
        first_index += counting_tasks[i].primitive_count;
        first_node  += counting_tasks[i].node_count;
    }
    size_t primitive_count = first_index;
    size_t* dst_primitive_indices = xmalloc(sizeof(size_t) * primitive_count);
    for (size_t i = 0; i < task_count; ++i) {
        rewrite_tasks[i].nodes = bvh->nodes;
        rewrite_tasks[i].src_primitive_indices = bvh->primitive_indices;
        rewrite_tasks[i].dst_primitive_indices = dst_primitive_indices;
    }
    submit_work(thread_pool, &rewrite_tasks[0].work_item, &rewrite_tasks[task_count - 1].work_item);
    wait_for_completion(thread_pool, 0);

    swap_primitive_indices(&bvh->primitive_indices, &dst_primitive_indices);

    free(dst_primitive_indices);
    free(counting_tasks);
    free(rewrite_tasks);
    free(primitive_counts);
    free(flags);
    free(parents);
}
