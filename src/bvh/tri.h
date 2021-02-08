#ifndef BVH_TRI_H
#define BVH_TRI_H

#include <stdbool.h>

#include "core/vec3.h"
#include "core/ray.h"

struct tri {
    struct vec3 p0;
    struct vec3 e1;
    struct vec3 e2;
    struct vec3 n;
};

static inline struct tri make_tri(
    const struct vec3* p0,
    const struct vec3* p1,
    const struct vec3* p2)
{
    struct vec3 e1 = sub_vec3(*p0, *p1);
    struct vec3 e2 = sub_vec3(*p2, *p0);
    struct vec3 n = cross_vec3(e1, e2);
    return (struct tri) { .p0 = *p0, .e1 = e1, .e2 = e2, .n = n };
}

static inline struct vec3 get_tri_p1(const struct tri* tri) {
    return sub_vec3(tri->p0, tri->e1);
}

static inline struct vec3 get_tri_p2(const struct tri* tri) {
    return add_vec3(tri->p0, tri->e2);
}

bool intersect_ray_tri(struct ray* ray, const struct tri* tri, struct hit* hit);

#endif
