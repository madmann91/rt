#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include "bvh/bvh.h"
#include "core/parallel.h"
#include "core/radix_sort.h"
#include "core/morton.h"
#include "core/alloc.h"

/* This construction algorithm is based on
 * "Parallel Locally-Ordered Clustering for Bounding Volume Hierarchy Construction",
 * by D. Meister and J. Bittner. 
 */

#define SEARCH_RADIUS 14

static inline size_t search_begin(size_t i) {
    return i > SEARCH_RADIUS ? i - SEARCH_RADIUS : 0;
}

static inline size_t search_end(size_t i, size_t n) {
    return i + SEARCH_RADIUS < n ? i + SEARCH_RADIUS : n;
}

struct centers_task {
    struct parallel_task task;
    center_fn_t center_fn;
    void* primitive_data;
    struct vec3* centers;
    struct bbox center_bbox;
};

static void run_centers_task(struct parallel_task* task) {
    struct centers_task* centers_task = (void*)task;
    for (size_t i = task->begin[0], n = task->end[0]; i < n; ++i) {
        struct vec3 center = centers_task->center_fn(centers_task->primitive_data, i);
        centers_task->centers[i] = center;
        centers_task->center_bbox = extend_bbox(centers_task->center_bbox, center);
    }
}

static void reduce_centers_task(struct parallel_task* left, const struct parallel_task* right) {
    ((struct centers_task*)left)->center_bbox = union_bbox(
        ((struct centers_task*)left)->center_bbox,
        ((const struct centers_task*)right)->center_bbox);
}

static void compute_centers(
    struct thread_pool* thread_pool,
    center_fn_t center_fn,
    void* primitive_data,
    struct vec3* centers,
    struct bbox* center_bbox,
    size_t primitive_count)
{
    struct centers_task centers_task = {
        .center_fn = center_fn,
        .primitive_data = primitive_data,
        .centers = centers,
        .center_bbox = empty_bbox()
    };
    reduce(
        thread_pool,
        run_centers_task,
        reduce_centers_task,
        &centers_task.task,
        sizeof(struct centers_task),
        (size_t[3]) { 0 },
        (size_t[3]) { primitive_count, 1, 1 });
    *center_bbox = centers_task.center_bbox;
}

struct morton_task {
    struct parallel_task task;
    morton_t* morton_codes;
    size_t* primitive_indices;
    const struct vec3* centers;
    const struct vec3* centers_min;
    const struct vec3* center_to_grid;
};

static void run_morton_task(struct parallel_task* task) {
    struct morton_task* morton_task = (void*)task;
    for (size_t i = task->begin[0], n = task->end[0]; i < n; ++i) {
        uint32_t x = (morton_task->centers[i]._[0] - morton_task->centers_min->_[0]) * morton_task->center_to_grid->_[0]; 
        uint32_t y = (morton_task->centers[i]._[1] - morton_task->centers_min->_[1]) * morton_task->center_to_grid->_[1]; 
        uint32_t z = (morton_task->centers[i]._[2] - morton_task->centers_min->_[2]) * morton_task->center_to_grid->_[2]; 
        morton_task->morton_codes[i] = morton_encode(x, y, z);
        morton_task->primitive_indices[i] = i;
    }
}

static inline void compute_morton_codes(
    struct thread_pool* thread_pool,
    morton_t** morton_codes,
    size_t** primitive_indices,
    center_fn_t center_fn,
    void* primitive_data,
    size_t primitive_count)
{
    struct bbox center_bbox;
    struct vec3* centers = xmalloc(sizeof(struct vec3) * primitive_count);
    compute_centers(
        thread_pool,
        center_fn, primitive_data,
        centers, &center_bbox, primitive_count);

    *morton_codes = xmalloc(sizeof(morton_t) * primitive_count);
    *primitive_indices = xmalloc(sizeof(size_t) * primitive_count);
    size_t grid_dim = 1 << (sizeof(morton_t) * CHAR_BIT / 3);
    struct vec3 centers_to_grid = div_vec3(
        const_vec3(grid_dim),
        sub_vec3(center_bbox.max, center_bbox.min));
    parallel_for(
        thread_pool,
        run_morton_task,
        (struct parallel_task*)&(struct morton_task) {
            .morton_codes = *morton_codes,
            .primitive_indices = *primitive_indices,
            .centers = centers,
            .centers_min = &center_bbox.min,
            .center_to_grid = &centers_to_grid
        },
        sizeof(struct morton_task),
        (size_t[3]) { 0 },
        (size_t[3]) { primitive_count, 1, 1 });
    free(centers);
}

static size_t* sort_morton_codes(
    struct thread_pool* thread_pool,
    morton_t* morton_codes,
    size_t* primitive_indices,
    size_t primitive_count)
{
    size_t* primitive_indices_copy = xmalloc(sizeof(size_t) * primitive_count);
    morton_t* morton_codes_copy    = xmalloc(sizeof(morton_t) * primitive_count);
    void* morton_src = morton_codes, *morton_dst = morton_codes_copy;
    radix_sort(
        thread_pool,
        &morton_src, &primitive_indices,
        &morton_dst, &primitive_indices_copy,
        sizeof(morton_t), primitive_count,
        sizeof(morton_t) * CHAR_BIT);
    free(morton_dst);
    free(primitive_indices_copy);
    morton_codes = morton_src;
    return primitive_indices;
}

struct leaves_task {
    struct parallel_task task;
    const size_t* primitive_indices;
    void* primitive_data;
    bbox_fn_t bbox_fn;
    struct bvh_node* leaves;
};

static void run_leaves_task(struct parallel_task* task) {
    struct leaves_task* leaf_node_task = (void*)task;
    for (size_t i = task->begin[0], n = task->end[0]; i < n; ++i) {
        struct bbox bbox = leaf_node_task->bbox_fn(
            leaf_node_task->primitive_data, leaf_node_task->primitive_indices[i]);
        struct bvh_node* leaf = &leaf_node_task->leaves[i];
        set_bvh_node_bbox(leaf, bbox);
        leaf->primitive_count = 1;
        leaf->first_child_or_primitive = i;
    }
}

struct neighbor_task {
    struct parallel_task task;
    const struct bvh_node* nodes;
    size_t* neighbors;
    size_t node_count;
};

static void run_neighbor_task(struct parallel_task* task) {
    struct neighbor_task* neighbor_task = (void*)task;
    for (size_t i = task->begin[0], n = task->end[0]; i < n; ++i) {
        size_t best_neighbor = -1;
        real_t best_distance = REAL_MAX;
        for (size_t j = search_begin(i), n = search_end(i, neighbor_task->node_count); j < n; ++j) {
            real_t distance = half_bbox_area(union_bbox(
                get_bvh_node_bbox(&neighbor_task->nodes[i]),
                get_bvh_node_bbox(&neighbor_task->nodes[j])));
            if (distance < best_distance) {
                best_distance = distance;
                best_neighbor = j;
            }
        }
        neighbor_task->neighbors[i] = best_neighbor;
    }
}

struct counting_task {
    struct work_item work_item;
    size_t begin, end;
    const size_t* neighbors;
    size_t merged_count;
    size_t unmerged_count;
};

static void run_counting_task(struct work_item* work_item) {
    struct counting_task* counting_task = (void*)work_item;
    for (size_t i = counting_task->begin, n = counting_task->end; i < n; ++i) {
        size_t j = counting_task->neighbors[i];
        if (counting_task->neighbors[j] == i)
            counting_task->merged_count += i < j ? 1 : 0;
        else
            counting_task->unmerged_count++;
    }
}

struct merge_task {
    struct work_item work_item;
    size_t begin, end;
    const size_t* neighbors;
    const struct bvh_node* src_unmerged_nodes;
    struct bvh_node* dst_unmerged_nodes;
    struct bvh_node* merged_nodes;
    size_t unmerged_index;
    size_t merged_index;
};

static void run_merge_task(struct work_item* work_item) {
    struct merge_task* merge_task = (void*)work_item;
    for (size_t i = merge_task->begin, n = merge_task->end; i < n; ++i) {
        size_t j = merge_task->neighbors[i];
        if (merge_task->neighbors[j] == i) {
            if (i < j) {
                struct bvh_node* unmerged_node =
                    &merge_task->dst_unmerged_nodes[merge_task->unmerged_index];
                size_t first_child = merge_task->merged_index;
                set_bvh_node_bbox(unmerged_node, union_bbox(
                    get_bvh_node_bbox(&merge_task->src_unmerged_nodes[i]),
                    get_bvh_node_bbox(&merge_task->src_unmerged_nodes[j])));
                unmerged_node->primitive_count = 0;
                unmerged_node->first_child_or_primitive = first_child;
                merge_task->merged_nodes[first_child + 0] = merge_task->src_unmerged_nodes[i];
                merge_task->merged_nodes[first_child + 1] = merge_task->src_unmerged_nodes[j];
                merge_task->unmerged_index++;
                merge_task->merged_index += 2;
            }
        } else {
            merge_task->dst_unmerged_nodes[merge_task->unmerged_index++] =
                merge_task->src_unmerged_nodes[i];
        }
    }
}

static void merge_nodes(
    struct thread_pool* thread_pool,
    struct bvh_node* src_unmerged_nodes,
    struct bvh_node* dst_unmerged_nodes,
    struct bvh_node* merged_nodes,
    size_t* neighbors,
    size_t* unmerged_count,
    size_t* merged_index)
{
    // Compute the neighbor array that contains the index
    // of the closest neighbor for each node.
    parallel_for(
        thread_pool,
        run_neighbor_task,
        (struct parallel_task*)&(struct neighbor_task) {
            .nodes = src_unmerged_nodes,
            .node_count = *unmerged_count,
            .neighbors = neighbors
        },
        sizeof(struct neighbor_task),
        (size_t[3]) { 0 },
        (size_t[3]) { *unmerged_count, 1, 1 });

    // Count how many nodes should be merged, and how many should not
    size_t thread_count = get_thread_count(thread_pool);
    size_t chunk_size = compute_chunk_size(*unmerged_count, thread_count);
    struct counting_task* counting_tasks = xmalloc(sizeof(struct counting_task) * thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        counting_tasks[i].work_item.work_fn = run_counting_task;
        counting_tasks[i].work_item.next = &counting_tasks[i + 1].work_item;
        counting_tasks[i].neighbors = neighbors;
        counting_tasks[i].begin = compute_chunk_begin(chunk_size, i, *unmerged_count);
        counting_tasks[i].end   = compute_chunk_end(chunk_size, i, *unmerged_count);
        counting_tasks[i].merged_count = 0;
        counting_tasks[i].unmerged_count = 0;
    }
    counting_tasks[thread_count - 1].work_item.next = NULL;
    submit_work(
        thread_pool,
        &counting_tasks[0].work_item,
        &counting_tasks[thread_count - 1].work_item);
    wait_for_completion(thread_pool, 0);

    size_t total_merged = 0;
    for (size_t i = 0; i < thread_count; ++i)
        total_merged += counting_tasks[i].merged_count;
    assert(total_merged > 0);

    // Merge nodes based on the results of the neighbor search
    *merged_index -= 2 * total_merged;
    size_t cur_merged_index = *merged_index;
    size_t cur_unmerged_index = 0;
    struct merge_task* merge_tasks = xmalloc(sizeof(struct merge_task) * thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        merge_tasks[i].work_item.work_fn = run_merge_task;
        merge_tasks[i].work_item.next = &merge_tasks[i + 1].work_item;
        merge_tasks[i].begin = counting_tasks[i].begin;
        merge_tasks[i].end   = counting_tasks[i].end;
        merge_tasks[i].merged_index   = cur_merged_index;
        merge_tasks[i].unmerged_index = cur_unmerged_index;
        merge_tasks[i].neighbors = neighbors;
        merge_tasks[i].src_unmerged_nodes = src_unmerged_nodes;
        merge_tasks[i].dst_unmerged_nodes = dst_unmerged_nodes;
        merge_tasks[i].merged_nodes = merged_nodes;
        submit_work(thread_pool, &merge_tasks[i].work_item, &merge_tasks[i].work_item);
        cur_merged_index   += counting_tasks[i].merged_count * 2;
        cur_unmerged_index += counting_tasks[i].merged_count + counting_tasks[i].unmerged_count;
    }
    *unmerged_count = cur_unmerged_index;

    free(counting_tasks);
    wait_for_completion(thread_pool, 0);

    free(merge_tasks);
}

SWAP(nodes, struct bvh_node*)

struct bvh build_bvh(
    struct thread_pool* thread_pool,
    void* primitive_data,
    bbox_fn_t bbox_fn,
    center_fn_t center_fn,
    size_t primitive_count)
{
    // Sort primitives by morton code
    size_t* primitive_indices;
    morton_t* morton_codes;
    compute_morton_codes(
        thread_pool,
        &morton_codes, &primitive_indices,
        center_fn, primitive_data, primitive_count);
    sort_morton_codes(
        thread_pool,
        morton_codes, primitive_indices,
        primitive_count);
    free(morton_codes);

    // Construct leaf nodes
    struct bvh_node* src_unmerged_nodes = xmalloc(sizeof(struct bvh_node) * primitive_count);
    struct bvh_node* dst_unmerged_nodes = xmalloc(sizeof(struct bvh_node) * primitive_count);
    parallel_for(
        thread_pool,
        run_leaves_task,
        (struct parallel_task*)&(struct leaves_task) {
            .primitive_indices = primitive_indices,
            .primitive_data = primitive_data,
            .bbox_fn = bbox_fn,
            .leaves = src_unmerged_nodes
        },
        sizeof(struct leaves_task),
        (size_t[3]) { 0 },
        (size_t[3]) { primitive_count, 1, 1 });

    // Merge nodes, level by level
    size_t node_count = 2 * primitive_count - 1;
    size_t* neighbors = xmalloc(sizeof(size_t) * primitive_count);
    struct bvh_node* merged_nodes = xmalloc(sizeof(struct bvh_node) * node_count);

    size_t unmerged_count = primitive_count;
    size_t merged_index = node_count - primitive_count;
    while (unmerged_count > 1) {
        merge_nodes(
            thread_pool,
            src_unmerged_nodes, dst_unmerged_nodes,
            merged_nodes, neighbors,
            &unmerged_count, &merged_index);

        swap_nodes(&src_unmerged_nodes, &dst_unmerged_nodes);
    }
    assert(unmerged_count == 1);
    merged_nodes[0] = src_unmerged_nodes[0];
    free(neighbors);
    free(src_unmerged_nodes);
    free(dst_unmerged_nodes);

    struct bvh bvh = {
        .nodes = merged_nodes,
        .primitive_indices = primitive_indices,
        .node_count = node_count
    };
    return bvh;
}
