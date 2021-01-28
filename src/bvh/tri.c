#include "bvh/tri.h"

bool intersect_ray_tri(struct ray* ray, const struct tri* tri, struct hit* hit) {
    struct vec3 c = sub_vec3(tri->p0, ray->org);
    struct vec3 r = cross_vec3(ray->dir, c);

    real_t inv_det = 1.0f / dot_vec3(tri->n, ray->dir);
    real_t u = dot_vec3(r, tri->e2) * inv_det;
    real_t v = dot_vec3(r, tri->e1) * inv_det;
    real_t w = 1.0f - u - v;

    // These comparisons are designed to return false
    // when one of t, u, or v is a NaN
    if (u >= 0 && v >= 0 && w >= 0) {
        real_t t = dot_vec3(tri->n, c) * inv_det;
        if (t >= ray->t_min && t <= ray->t_max) {
            ray->t_max = t;
            hit->u = u;
            hit->v = v;
            return true;
        }
    }

    return false;
}
