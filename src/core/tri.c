#include "core/tri.h"

bool intersect_ray_tri(struct ray* ray, struct hit* hit, const struct tri* tri) {
    struct vec3 c = sub_vec3(tri->p0, ray->org);
    struct vec3 r = cross_vec3(ray->dir, c);

    real_t inv_det = ((real_t)1) / dot_vec3(tri->n, ray->dir);
    real_t u = dot_vec3(r, tri->e2) * inv_det;
    real_t v = dot_vec3(r, tri->e1) * inv_det;

    // These comparisons are designed to return false
    // when one of t, u, or v is a NaN
    if (u >= 0 && v >= 0 && u + v <= 1) {
        real_t t = dot_vec3(tri->n, c) * inv_det;
        if (t >= ray->t_min && t <= ray->t_max) {
            ray->t_max = t;
            hit->uv = make_vec2(u, v);
            return true;
        }
    }

    return false;
}
