#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include "bvh/bvh.h"
#include "core/vec3.h"
#include "core/parallel.h"
#include "core/radix_sort.h"
#include "core/morton.h"
#include "core/alloc.h"
#include "core/utils.h"

/* This construction algorithm is based on
 * "Parallel Locally-Ordered Clustering for Bounding Volume Hierarchy Construction",
 * by D. Meister and J. Bittner. 
 */

#define SEARCH_RADIUS 14

static inline size_t search_begin(size_t i) {
    return i > SEARCH_RADIUS ? i - SEARCH_RADIUS : 0;
}

static inline size_t search_end(size_t i, size_t n) {
    return i + SEARCH_RADIUS + 1 < n ? i + SEARCH_RADIUS + 1 : n;
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

static inline morton_t real_to_grid(real_t x) {
    return x < 0 ? 0 : (x > MORTON_GRID_DIM - 1 ? MORTON_GRID_DIM - 1 : x);
}

static void run_morton_task(struct parallel_task* task) {
    struct morton_task* morton_task = (void*)task;
    for (size_t i = task->begin[0], n = task->end[0]; i < n; ++i) {
        struct vec3 v = mul_vec3(
            sub_vec3(morton_task->centers[i], *morton_task->centers_min),
            *morton_task->center_to_grid);
        morton_task->morton_codes[i] = morton_encode(
            real_to_grid(v._[0]),
            real_to_grid(v._[1]),
            real_to_grid(v._[2]));
        morton_task->primitive_indices[i] = i;
    }
}

static inline void compute_morton_codes(
    struct thread_pool* thread_pool,
    morton_t* morton_codes,
    size_t* primitive_indices,
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

    struct vec3 centers_to_grid = div_vec3(
        const_vec3(MORTON_GRID_DIM),
        sub_vec3(center_bbox.max, center_bbox.min));
    parallel_for(
        thread_pool,
        run_morton_task,
        (struct parallel_task*)&(struct morton_task) {
            .morton_codes = morton_codes,
            .primitive_indices = primitive_indices,
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
        size_t best_neighbor = SIZE_MAX;
        real_t best_distance = REAL_MAX;
        struct bbox this_bbox = get_bvh_node_bbox(&neighbor_task->nodes[i]);
        for (size_t j = search_begin(i), n = search_end(i, neighbor_task->node_count); j < n; ++j) {
            if (j == i)
                continue;
            struct bbox other_bbox = get_bvh_node_bbox(&neighbor_task->nodes[j]);
            real_t distance = half_bbox_area(union_bbox(this_bbox, other_bbox));
            assert(isfinite(distance));
            if (distance < best_distance) {
                best_distance = distance;
                best_neighbor = j;
            }
        }
        assert(best_neighbor != SIZE_MAX);
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
    counting_task->merged_count = counting_task->unmerged_count = 0;
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
    const struct bvh_node* src_unmerged_nodes,
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
    size_t task_count = get_thread_count(thread_pool);
    size_t chunk_size = compute_chunk_size(*unmerged_count, task_count);
    struct counting_task* counting_tasks = xmalloc(sizeof(struct counting_task) * task_count);
    for (size_t i = 0; i < task_count; ++i) {
        counting_tasks[i].work_item.work_fn = run_counting_task;
        counting_tasks[i].work_item.next = &counting_tasks[i + 1].work_item;
        counting_tasks[i].neighbors = neighbors;
        counting_tasks[i].begin = compute_chunk_begin(chunk_size, i);
        counting_tasks[i].end   = compute_chunk_end(chunk_size, i, *unmerged_count);
    }
    counting_tasks[task_count - 1].work_item.next = NULL;
    submit_work(thread_pool, &counting_tasks[0].work_item, &counting_tasks[task_count - 1].work_item);
    wait_for_completion(thread_pool, 0);

    size_t total_merged = 0;
    for (size_t i = 0; i < task_count; ++i)
        total_merged += counting_tasks[i].merged_count;
    assert(total_merged > 0);

    // Merge nodes based on the results of the neighbor search
    assert(*merged_index > 2 * total_merged);
    *merged_index -= 2 * total_merged;
    size_t cur_merged_index = *merged_index;
    size_t cur_unmerged_index = 0;
    struct merge_task* merge_tasks = xmalloc(sizeof(struct merge_task) * task_count);
    for (size_t i = 0; i < task_count; ++i) {
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
        cur_merged_index   += counting_tasks[i].merged_count * 2;
        cur_unmerged_index += counting_tasks[i].merged_count + counting_tasks[i].unmerged_count;
    }
    merge_tasks[task_count - 1].work_item.next = NULL;
    submit_work(thread_pool, &merge_tasks[0].work_item, &merge_tasks[task_count - 1].work_item);

    *unmerged_count = cur_unmerged_index;
    wait_for_completion(thread_pool, 0);

    free(merge_tasks);
    free(counting_tasks);
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
    morton_t* morton_codes = xmalloc(sizeof(morton_t) * primitive_count);
    size_t* primitive_indices = xmalloc(sizeof(size_t) * primitive_count);
    compute_morton_codes(
        thread_pool,
        morton_codes, primitive_indices,
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
    size_t merged_index = node_count;
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

void free_bvh(struct bvh* bvh) {
    free(bvh->nodes);
    free(bvh->primitive_indices);
    bvh->nodes = NULL;
    bvh->primitive_indices = NULL;
}

/* The robust traversal implementation is inspired from T. Ize's "Robust BVH Ray Traversal"
 * article. It is only enabled when USE_ROBUST_BVH_TRAVERSAL is defined.
 */

struct ray_data {
#ifdef USE_ROBUST_BVH_TRAVERSAL
    struct vec3 inv_dir;
    struct vec3 padded_inv_dir;
#else
    struct vec3 inv_dir;
    struct vec3 scaled_org;
#endif
    int octant[3];
};

static inline real_t intersect_axis_min(
    int axis, real_t p,
    const struct ray* ray,
    const struct ray_data* ray_data)
{
#ifdef USE_ROBUST_BVH_TRAVERSAL
    return (p - ray->org._[axis]) * ray_data->inv_dir._[axis];
#else
    (void)ray;
    return fast_mul_add(p, ray_data->inv_dir._[axis], ray_data->scaled_org._[axis]); 
#endif
}

static inline real_t intersect_axis_max(
    int axis, real_t p,
    const struct ray* ray,
    const struct ray_data* ray_data)
{
#ifdef USE_ROBUST_BVH_TRAVERSAL
    return (p - ray->org._[axis]) * ray_data->padded_inv_dir._[axis];
#else
    return intersect_axis_min(axis, p, ray, ray_data);
#endif
}

static inline void compute_ray_data(const struct ray* ray, struct ray_data* ray_data) {
#ifdef USE_ROBUST_BVH_TRAVERSAL
    ray_data->inv_dir._[0] = safe_inverse(ray->dir._[0]);
    ray_data->inv_dir._[1] = safe_inverse(ray->dir._[1]);
    ray_data->inv_dir._[2] = safe_inverse(ray->dir._[2]);
    ray_data->padded_inv_dir._[0] = add_ulp_magnitude(ray_data->inv_dir._[0], 2);
    ray_data->padded_inv_dir._[1] = add_ulp_magnitude(ray_data->inv_dir._[1], 2);
    ray_data->padded_inv_dir._[2] = add_ulp_magnitude(ray_data->inv_dir._[2], 2);
#else
    ray_data->inv_dir._[0] = safe_inverse(ray->dir._[0]);
    ray_data->inv_dir._[1] = safe_inverse(ray->dir._[1]);
    ray_data->inv_dir._[2] = safe_inverse(ray->dir._[2]);
    ray_data->scaled_org._[0] = -ray->dir._[0] * ray_data->inv_dir._[0];
    ray_data->scaled_org._[1] = -ray->dir._[1] * ray_data->inv_dir._[1];
    ray_data->scaled_org._[2] = -ray->dir._[2] * ray_data->inv_dir._[2];
#endif
    ray_data->octant[0] = signbit(ray->dir._[0]);
    ray_data->octant[1] = signbit(ray->dir._[1]);
    ray_data->octant[2] = signbit(ray->dir._[2]);
}

static inline bool intersect_node(
    const struct ray* ray,
    const struct ray_data* ray_data,
    const struct bvh_node* node,
    real_t* t_entry)
{
    real_t tmin_x = intersect_axis_min(0, node->bounds[0 + ray_data->octant[0]], ray, ray_data);
    real_t tmin_y = intersect_axis_min(1, node->bounds[2 + ray_data->octant[1]], ray, ray_data);
    real_t tmin_z = intersect_axis_min(2, node->bounds[4 + ray_data->octant[2]], ray, ray_data);
    real_t tmax_x = intersect_axis_max(0, node->bounds[0 + 1 - ray_data->octant[0]], ray, ray_data);
    real_t tmax_y = intersect_axis_max(1, node->bounds[2 + 1 - ray_data->octant[1]], ray, ray_data);
    real_t tmax_z = intersect_axis_max(2, node->bounds[4 + 1 - ray_data->octant[2]], ray, ray_data);

    real_t tmin = max_real(max_real(tmin_x, tmin_y), max_real(tmin_z, ray->t_min));
    real_t tmax = min_real(min_real(tmax_x, tmax_y), min_real(tmax_z, ray->t_max));

    *t_entry = tmin;
    return tmin <= tmax;
}

bool intersect_bvh(
    void* intersection_data,
    intersect_leaf_fn_t intersect_leaf,
    const struct bvh* bvh, 
    struct ray* ray, struct hit* hit, bool any)
{
    struct ray_data ray_data;
    compute_ray_data(ray, &ray_data);

    // Special case when the root node is a leaf
    if (unlikely(bvh->nodes->primitive_count > 0)) {
        real_t t_entry;
        return
            intersect_node(ray, &ray_data, bvh->nodes, &t_entry) &&
            intersect_leaf(intersection_data, bvh->nodes, ray, hit);
    }

    // General case
    bits_t stack[bvh->depth];
    size_t stack_size = 0;

    const struct bvh_node* left = bvh->nodes + bvh->nodes->first_child_or_primitive;
    while (true) {
        const struct bvh_node* right = left + 1;

        // Intersect the two children together
        real_t t_entry[2];
        bool hit_left  = intersect_node(ray, &ray_data, left,  t_entry + 0);
        bool hit_right = intersect_node(ray, &ray_data, right, t_entry + 1);

#define INTERSECT_CHILD(child) \
        if (hit_##child) { \
            if (unlikely(child->primitive_count > 0)) { \
                if (intersect_leaf(intersection_data, child, ray, hit) && any) \
                    return true; \
                child = NULL; \
            } \
        } else \
            child = NULL;

        INTERSECT_CHILD(left)
        INTERSECT_CHILD(right)

#undef INTERSECT_CHILD

        if (left) {
            // The left child was intersected
            if (right) {
                // Both children were intersected, we need to sort them based
                // on their distances (only in closest intersection mode).
                if (!any && t_entry[0] > t_entry[1]) {
                    const struct bvh_node* tmp = left;
                    left = right;
                    right = tmp;
                }
                stack[stack_size++] = right->first_child_or_primitive;
            }
            left = bvh->nodes + left->first_child_or_primitive;
        } else if (right) {
            // Only the right child was intersected
            left = bvh->nodes + right->first_child_or_primitive;
        } else {
            // No intersection was found
            if (stack_size == 0)
                break;
            left = bvh->nodes + stack[--stack_size];
        }
    }

    return hit->primitive_index != INVALID_PRIMITIVE_INDEX;
}
