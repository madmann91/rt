#ifndef CORE_QUAD_H
#define CORE_QUAD_H

#include <stdbool.h>

#include "core/vec3.h"
#include "core/ray.h"

struct quad {
    struct vec3 p0;
    struct vec3 e1;
    struct vec3 e2;
    struct vec3 e3;
    struct vec3 e4;
    struct vec3 n;
};

// A quad is interpreted as two triangles: p0, p1, p3 and p2, p3, p1
static inline struct quad make_quad(
    const struct vec3* p0,
    const struct vec3* p1,
    const struct vec3* p2,
    const struct vec3* p3)
{
    struct vec3 e1 = sub_vec3(*p0, *p1);
    struct vec3 e2 = sub_vec3(*p2, *p0);
    struct vec3 e3 = sub_vec3(*p2, *p3);
    struct vec3 e4 = sub_vec3(*p1, *p2);
    struct vec3 n = cross_vec3(e1, e2);
    return (struct quad) {
        .p0 = *p0, 
        .e1 = e1,
        .e2 = e2,
        .e3 = e3,
        .e4 = e4,
        .n = n
    };
}

static inline struct vec3 get_quad_p1(const struct quad* quad) {
    return sub_vec3(quad->p0, quad->e1);
}

static inline struct vec3 get_quad_p2(const struct quad* quad) {
    return add_vec3(quad->p0, quad->e2);
}

static inline struct vec3 get_quad_p3(const struct quad* quad) {
    return add_vec3(quad->p0, quad->e3);
}

bool intersect_ray_quad(struct ray* ray, struct hit*, const struct quad* tri);

#endif
