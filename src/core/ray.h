#ifndef CORE_RAY_H
#define CORE_RAY_H

#include <stddef.h>
#include <stdint.h>

#include "core/vec3.h"
#include "core/utils.h"

#define INVALID_PRIMITIVE_INDEX SIZE_MAX

struct ray {
    struct vec3 org;
    struct vec3 dir;
    real_t t_min, t_max;
};

struct hit {
    size_t primitive_index;
    real_t u, v;
};

static inline struct vec3 point_at(struct ray ray, real_t t) {
    return (struct vec3) {
        {
            fast_mul_add(ray.dir._[0], t, ray.org._[0]),
            fast_mul_add(ray.dir._[1], t, ray.org._[1]),
            fast_mul_add(ray.dir._[2], t, ray.org._[2]),
        }
    };
}

#endif
