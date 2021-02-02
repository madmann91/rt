#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#include "bvh/bvh.h"
#include "core/vec3.h"
#include "core/utils.h"
#include "core/parallel_for.h"
#include "core/alloc.h"
#include "core/morton.h"
#include "core/radix_sort.h"

// BVH construction ---------------------------------------------------------------------

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

static inline struct bbox get_node_bbox(const struct bvh_node* node) {
    return (struct bbox) {
        .min = (struct vec3) { { node->bounds[0], node->bounds[2], node->bounds[4] } },
        .max = (struct vec3) { { node->bounds[1], node->bounds[3], node->bounds[5] } },
    };
}

struct compute_centers_task {
    struct parallel_task task;
    center_fn_t center_fn;
    void* primitive_data;
    struct vec3* centers;
    struct bbox center_bbox;
};

static void compute_centers_task(struct parallel_task* task) {
    struct compute_centers_task* centers_task = (void*)task;
    for (size_t i = task->begin[0], n = task->end[0]; i < n; ++i) {
        struct vec3 center = centers_task->center_fn(centers_task->primitive_data, i);
        centers_task->centers[i] = center;
        centers_task->center_bbox = extend_bbox(centers_task->center_bbox, center);
    }
}

static struct vec3* compute_centers(
    struct thread_pool* thread_pool,
    struct mem_pool** mem_pool,
    center_fn_t center_fn,
    void* primitive_data,
    struct bbox* center_bbox,
    size_t primitive_count)
{
    size_t prev_used_mem = get_used_mem(*mem_pool);
    struct vec3* centers = xmalloc(sizeof(struct vec3) * primitive_count);
    struct compute_centers_task* center_task = (void*)parallel_for(
        thread_pool, mem_pool,
        compute_centers_task,
        &(&(struct compute_centers_task) {
            .center_fn = center_fn,
            .primitive_data = primitive_data,
            .centers = centers,
            .center_bbox = empty_bbox()
        })->task,
        sizeof(struct compute_centers_task),
        (size_t[3]) { 0 },
        (size_t[3]) { primitive_count, 1, 1 });
    // Compute the bounding box of all the primitive centers
    *center_bbox = empty_bbox();
    while (center_task) {
        *center_bbox = union_bbox(*center_bbox, center_task->center_bbox);
        center_task = (struct compute_centers_task*)center_task->task.work_item.next;
    }
    reset_mem_pool(mem_pool, prev_used_mem);
    return centers;
}

struct morton_code_task {
    struct parallel_task task;
    morton_t* morton_codes;
    size_t* primitive_indices;
    const struct vec3* centers;
    const struct vec3* centers_min;
    const struct vec3* center_to_grid;
};

static void morton_code_task(struct parallel_task* task) {
    struct morton_code_task* morton_task = (void*)task;
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
    struct mem_pool** mem_pool,
    morton_t** morton_codes,
    size_t** primitive_indices,
    center_fn_t center_fn,
    void* primitive_data,
    size_t primitive_count)
{
    size_t prev_used_mem = get_used_mem(*mem_pool);
    struct bbox center_bbox;
    struct vec3* centers = compute_centers(
        thread_pool, mem_pool,
        center_fn, primitive_data,
        &center_bbox, primitive_count);

    *morton_codes = xmalloc(sizeof(morton_t) * primitive_count);
    *primitive_indices = xmalloc(sizeof(size_t) * primitive_count);
    size_t grid_dim = 1 << (sizeof(morton_t) * CHAR_BIT / 3);
    struct vec3 centers_to_grid = div_vec3(
        const_vec3(grid_dim),
        sub_vec3(center_bbox.max, center_bbox.min));
    parallel_for(
        thread_pool, mem_pool,
        morton_code_task,
        (struct parallel_task*)&(struct morton_code_task) {
            .morton_codes = *morton_codes,
            .primitive_indices = *primitive_indices,
            .centers = centers,
            .centers_min = &center_bbox.min,
            .center_to_grid = &centers_to_grid
        },
        sizeof(struct morton_code_task),
        (size_t[3]) { 0 },
        (size_t[3]) { primitive_count, 1, 1 });
    reset_mem_pool(mem_pool, prev_used_mem);
    free(centers);
}

static size_t* sort_morton_codes(
    struct thread_pool* thread_pool,
    struct mem_pool** mem_pool,
    morton_t* morton_codes,
    size_t* primitive_indices,
    size_t primitive_count)
{
    size_t* primitive_indices_copy = xmalloc(sizeof(size_t) * primitive_count);
    morton_t* morton_codes_copy    = xmalloc(sizeof(morton_t) * primitive_count);
    void* morton_src = morton_codes, *morton_dst = morton_codes_copy;
    radix_sort(
        thread_pool, mem_pool,
        &morton_src, &primitive_indices,
        &morton_dst, &primitive_indices_copy,
        sizeof(morton_t), primitive_count,
        sizeof(morton_t) * CHAR_BIT);
    free(morton_dst);
    free(primitive_indices_copy);
    morton_codes = morton_src;
    return primitive_indices;
}

struct leaf_node_task {
    struct parallel_task task;
    const size_t* primitive_indices;
    void* primitive_data;
    bbox_fn_t bbox_fn;
    struct bvh_node* leaves;
};

static void leaf_node_task(struct parallel_task* task) {
    struct leaf_node_task* leaf_node_task = (void*)task;
    for (size_t i = task->begin[0], n = task->end[0]; i < n; ++i) {
        struct bbox bbox = leaf_node_task->bbox_fn(
            leaf_node_task->primitive_data, leaf_node_task->primitive_indices[i]);
        struct bvh_node* leaf = &leaf_node_task->leaves[i];
        leaf->bounds[0] = bbox.min._[0];
        leaf->bounds[1] = bbox.max._[0];
        leaf->bounds[2] = bbox.min._[1];
        leaf->bounds[3] = bbox.max._[1];
        leaf->bounds[4] = bbox.min._[2];
        leaf->bounds[5] = bbox.max._[2];
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

static void neighbor_task(struct parallel_task* task) {
    struct neighbor_task* neighbor_task = (void*)task;
    for (size_t i = task->begin[0], n = task->end[0]; i < n; ++i) {
        size_t best_neighbor = -1;
        real_t best_distance = REAL_MAX;
        for (size_t j = search_begin(i), n = search_end(i, neighbor_task->node_count); j < n; ++j) {
            real_t distance = half_bbox_area(union_bbox(
                get_node_bbox(&neighbor_task->nodes[i]),
                get_node_bbox(&neighbor_task->nodes[j])));
            if (distance < best_distance) {
                best_distance = distance;
                best_neighbor = j;
            }
        }
        neighbor_task->neighbors[i] = best_neighbor;
    }
}

static void merge_nodes(
    struct thread_pool* thread_pool,
    struct mem_pool** mem_pool,
    struct bvh_node** src_unmerged_nodes,
    struct bvh_node** dst_unmerged_nodes,
    size_t* unmerged_count,
    struct bvh_node* merged_nodes,
    size_t* merged_index,
    const size_t* neighbors)
{
    // Count how many nodes are merged
    // Perform a prefix sum to find where to place merged nodes
    // Copy merged nodes into the array
}

SWAP(nodes, struct bvh_node*)

struct bvh build_bvh(
    struct thread_pool* thread_pool,
    struct mem_pool** mem_pool,
    void* primitive_data,
    bbox_fn_t bbox_fn,
    center_fn_t center_fn,
    size_t primitive_count)
{
    // Sort primitives by morton code
    size_t* primitive_indices;
    morton_t* morton_codes;
    compute_morton_codes(
        thread_pool, mem_pool,
        &morton_codes, &primitive_indices,
        center_fn, primitive_data, primitive_count);
    sort_morton_codes(
        thread_pool, mem_pool,
        morton_codes, primitive_indices,
        primitive_count);
    free(morton_codes);

    // Construct leaf nodes
    struct bvh_node* unmerged_nodes = xmalloc(sizeof(struct bvh_node) * primitive_count);
    parallel_for(
        thread_pool, mem_pool,
        leaf_node_task,
        (struct parallel_task*)&(struct leaf_node_task) {
            .primitive_indices = primitive_indices,
            .primitive_data = primitive_data,
            .bbox_fn = bbox_fn,
            .leaves = unmerged_nodes
        },
        sizeof(struct morton_code_task),
        (size_t[3]) { 0 },
        (size_t[3]) { primitive_count, 1, 1 });


    // Merge nodes, level by level
    size_t node_count = 2 * primitive_count - 1;
    size_t* neighbors = xmalloc(sizeof(size_t) * primitive_count);
    struct bvh_node* merged_nodes = xmalloc(sizeof(struct bvh_node) * node_count);
    struct bvh_node* unmerged_nodes_copy = xmalloc(sizeof(struct bvh_node) * primitive_count);

    size_t unmerged_count = primitive_count;
    size_t merged_index = node_count - primitive_count;
    while (unmerged_count > 1) {
        // Fill in the neighbor array
        parallel_for(
            thread_pool, mem_pool,
            neighbor_task,
            (struct parallel_task*)&(struct neighbor_task) {
                .nodes = unmerged_nodes,
                .node_count = unmerged_count,
                .neighbors = neighbors
            },
            sizeof(struct morton_code_task),
            (size_t[3]) { 0 },
            (size_t[3]) { primitive_count, 1, 1 });

        // Merge nodes using the computed neighbor array
        merge_nodes(
            thread_pool, mem_pool,
            &unmerged_nodes, &unmerged_nodes_copy, &unmerged_count,
            merged_nodes, &merged_index,
            neighbors);

        swap_nodes(&unmerged_nodes, &unmerged_nodes_copy);
    }
    free(neighbors);
    free(unmerged_nodes);

    return (struct bvh) {
        .nodes = merged_nodes,
        .primitive_indices = primitive_indices
    };
}

// Ray traversal ------------------------------------------------------------------------

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
