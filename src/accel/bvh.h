#ifndef ACCEL_BVH_H
#define ACCEL_BVH_H

#include <stdbool.h>

#include "core/config.h"
#include "core/bbox.h"

struct ray;
struct hit;
struct thread_pool;

struct bvh_node {
    real_t bounds[6];        // Stored as min_x, max_x, min_y, max_y, ...
    bits_t primitive_count;  // A primitive count of 0 indicates an inner node
    bits_t first_child_or_primitive;
};

struct bvh {
    struct bvh_node* nodes;    // The root is located at nodes[0]
    size_t* primitive_indices; // Reordered primitive indices such that leaves index into that array.
    size_t node_count;
};

// Bounding box and cent callbacks used by the construction algorithm
// to obtain the bounding box and center of a primitive, respectively.
typedef struct bbox (*bbox_fn_t)(void* primitive_data, size_t index);
typedef struct vec3 (*center_fn_t)(void* primitive_data, size_t index);

static inline struct bbox get_bvh_node_bbox(const struct bvh_node* node) {
    return (struct bbox) {
        .min = (struct vec3) { { node->bounds[0], node->bounds[2], node->bounds[4] } },
        .max = (struct vec3) { { node->bounds[1], node->bounds[3], node->bounds[5] } },
    };
}

static inline void set_bvh_node_bbox(struct bvh_node* node, const struct bbox* bbox) {
    node->bounds[0] = bbox->min._[0];
    node->bounds[1] = bbox->max._[0];
    node->bounds[2] = bbox->min._[1];
    node->bounds[3] = bbox->max._[1];
    node->bounds[4] = bbox->min._[2];
    node->bounds[5] = bbox->max._[2];
}

/*
 * Builds a BVH for a set of primitives with the given
 * bounding boxes and centers. The thread pool is used to
 * issue work to multiple threads. The traversal cost
 * is expressed as a ratio of the cost of traversing a node vs.
 * the cost of intersecting a primitive.
 */
struct bvh* build_bvh(
    struct thread_pool* thread_pool,
    void* primitive_data,
    bbox_fn_t bbox_fn,
    center_fn_t center_fn,
    size_t primitive_count,
    real_t traversal_cost);

void free_bvh(struct bvh*);

/*
 * Intersection callback used by the traversal function
 * to intersect the contents of a leaf.
 * The parameter `intersection_data` is the same as the
 * argument passed to `intersect_bvh()`.
 */
typedef bool (*intersect_ray_leaf_fn_t)(
    struct ray* ray, struct hit*,
    const struct bvh_node* leaf,
    void* intersection_data);

/*
 * Intersects a BVH with a ray, using the given callback
 * to intersect the primitives in a leaf. If `any` is set,
 * then the algorithm terminates as soon as an intersection
 * is found. Otherwise, the algorithms searches for the closest
 * intersection. If an intersection was found, `ray->t_max`
 * contains the intersection distance, and `hit` contains the
 * hit data. Otherwise, `ray` and `hit` are left unchanged.
 */
bool intersect_ray_bvh(
    struct ray*, struct hit*,
    const struct bvh* bvh,
    intersect_ray_leaf_fn_t intersect_ray_leaf,
    void* intersection_data, bool any);

#endif
