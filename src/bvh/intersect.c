#include "bvh/bvh.h"
#include "core/vec3.h"
#include "core/utils.h"

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
