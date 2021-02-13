#include "core/quad.h"

bool intersect_ray_quad(struct ray* ray, struct hit* hit, const struct quad* quad) {
    struct vec3 c = sub_vec3(quad->p0, ray->org);
    struct vec3 r = cross_vec3(ray->dir, c);

    real_t inv_det = ((real_t)1) / dot_vec3(quad->n, ray->dir);
    real_t u, v, t;

    real_t u1 = dot_vec3(r, quad->e2) * inv_det;
    real_t v1 = dot_vec3(r, quad->e1) * inv_det;
    if (u1 >= 0 && v1 >= 0 && u1 + v1 <= 1) {
        u = u1, v = v1;
        goto hit;
    }

    real_t u2 = dot_vec3(r, quad->e4) * inv_det;
    real_t v2 = dot_vec3(r, quad->e3) * inv_det;
    if (u2 >= 0 && v2 >= 0 && u2 + v2 <= 1) {
        u = 1 - u2, v = 1 - v2;
        goto hit;
    }
    return false;

hit:
    t = dot_vec3(quad->n, c) * inv_det;
    if (t >= ray->t_min && t <= ray->t_max) {
        ray->t_max = t;
        hit->uv = make_vec2(u, v);
        return true;
    }
    return false;
}
