#ifndef CORE_VEC3_H
#define CORE_VEC3_H

#include "core/config.h"
#include "core/utils.h"

struct vec3 { real_t _[3]; };

static inline struct vec3 const_vec3(real_t x) {
    return (struct vec3) { { x, x, x } };
}

static inline struct vec3 add_vec3(struct vec3 a, struct vec3 b) {
    return (struct vec3) { { a._[0] + b._[0], a._[1] + b._[1], a._[2] + b._[2] } };
}

static inline struct vec3 sub_vec3(struct vec3 a, struct vec3 b) {
    return (struct vec3) { { a._[0] - b._[0], a._[1] - b._[1], a._[2] - b._[2] } };
}

static inline struct vec3 mul_vec3(struct vec3 a, struct vec3 b) {
    return (struct vec3) { { a._[0] * b._[0], a._[1] * b._[1], a._[2] * b._[2] } };
}

static inline struct vec3 div_vec3(struct vec3 a, struct vec3 b) {
    return (struct vec3) { { a._[0] / b._[0], a._[1] / b._[1], a._[2] / b._[2] } };
}

static inline struct vec3 min_vec3(struct vec3 a, struct vec3 b) {
    return (struct vec3) {
        {
            min_real(a._[0], b._[0]),
            min_real(a._[1], b._[1]),
            min_real(a._[2], b._[2]),
        }
    };
}

static inline struct vec3 max_vec3(struct vec3 a, struct vec3 b) {
    return (struct vec3) {
        {
            max_real(a._[0], b._[0]),
            max_real(a._[1], b._[1]),
            max_real(a._[2], b._[2]),
        }
    };
}

static inline struct vec3 scale_vec3(struct vec3 a, real_t f) {
    return (struct vec3) { { a._[0] * f, a._[1] * f, a._[2] * f } };
}

static inline struct vec3 neg_vec3(struct vec3 a) {
    return (struct vec3) { { -a._[0], -a._[1], -a._[2] } };
}

static inline real_t dot_vec3(struct vec3 a, struct vec3 b) {
    return fast_mul_add(a._[0], b._[0], fast_mul_add(a._[1], b._[1], a._[2] * b._[2]));
}

static inline real_t lensq_vec3(struct vec3 a) {
    return dot_vec3(a, a);
}

static inline real_t len_vec3(struct vec3 a) {
    return sqrt(lensq_vec3(a));
}

static inline struct vec3 normalize_vec3(struct vec3 a) {
    return scale_vec3(a, ((real_t)1) / len_vec3(a));
}

static inline struct vec3 cross_vec3(struct vec3 a, struct vec3 b) {
    return (struct vec3) {
        {
            a._[1] * b._[2] - a._[2] * b._[1],
            a._[2] * b._[0] - a._[0] * b._[2],
            a._[0] * b._[1] - a._[1] * b._[0]
        }
    };
}

#endif
