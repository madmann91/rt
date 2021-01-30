#ifndef BVH_BVH_H
#define BVH_BVH_H

#include <stdbool.h>

#include "core/config.h"
#include "core/thread_pool.h"
#include "core/mem_pool.h"
#include "core/ray.h"
#include "core/bbox.h"

struct bvh_node {
    real_t bounds[6];        // Stored as min_x, max_x, min_y, max_y, ...
    bits_t primitive_count;  // A primitive count of 0 indicates an inner node
    bits_t first_child_or_primitive;
};

struct bvh {
    struct bvh_node* nodes;    // The root is located at nodes[0]
    size_t* primitive_indices; // Reordered primitive indices such that leaves index into that array.
    size_t depth;              // By convention, 0 means that the root is a leaf
};

/* Bounding box and cent callbacks used by the construction algorithm
 * to obtain the bounding box and center of a primitive, respectively.
 */
typedef void (*bbox_fn_t)(
    void* primitive_data,
    size_t begin, size_t end,
    struct bbox* bboxes,
    size_t stride);
typedef void (*center_fn_t)(
    void* primitive_data,
    size_t begin, size_t end,
    struct vec3* centers,
    size_t stride);

/* Builds a BVH for a set of primitives with the given
 * bounding boxes and centers. The thread pool is used to
 * issue work to multiple threads.
 */
struct bvh build_bvh(
    struct thread_pool* thread_pool,
    struct mem_pool** mem_pool,
    void* primitive_data,
    bbox_fn_t bbox_fn,
    center_fn_t center_fn,
    size_t primitive_count);

/* Intersection callback used by the traversal function
 * to intersect the contents of a leaf.
 * The parameter `intersection_data` is the same as the
 * argument passed to `intersect_bvh()`.
 */
typedef bool (*intersect_leaf_fn_t)(
    void* intersection_data,
    const struct bvh_node* leaf,
    struct ray* ray, struct hit* hit);

/* Intersects a BVH with a ray, using the given callback
 * to intersect the primitives in a leaf. If `any` is set,
 * then the algorithm terminates as soon as an intersection
 * is found. Otherwise, the algorithms searches for the closest
 * intersection. This function returns true if an intersection was
 * found, otherwise false. After a call to this function, `ray->t_max`
 * contains the intersection distance, and `hit` contains the
 * hit data (if an intersection was found).
 */
bool intersect_bvh(
    void* intersection_data,
    intersect_leaf_fn_t intersect_leaf,
    const struct bvh* bvh,
    struct ray* ray, struct hit* hit, bool any);

#endif
