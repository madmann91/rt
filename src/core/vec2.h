#ifndef CORE_VEC2_H
#define CORE_VEC2_H

#include "core/config.h"
#include "core/utils.h"

struct vec2 { real_t _[2]; };

static inline struct vec2 const_vec2(real_t x) {
    return (struct vec2) { { x, x } };
}

static inline struct vec2 add_vec2(struct vec2 a, struct vec2 b) {
    return (struct vec2) { { a._[0] + b._[0], a._[1] + b._[1] } };
}

static inline struct vec2 sub_vec2(struct vec2 a, struct vec2 b) {
    return (struct vec2) { { a._[0] - b._[0], a._[1] - b._[1] } };
}

static inline struct vec2 mul_vec2(struct vec2 a, struct vec2 b) {
    return (struct vec2) { { a._[0] * b._[0], a._[1] * b._[1] } };
}

static inline struct vec2 div_vec2(struct vec2 a, struct vec2 b) {
    return (struct vec2) { { a._[0] / b._[0], a._[1] / b._[1] } };
}

static inline struct vec2 min_vec2(struct vec2 a, struct vec2 b) {
    return (struct vec2) { { min_real(a._[0], b._[0]), min_real(a._[1], b._[1]) } };
}

static inline struct vec2 max_vec2(struct vec2 a, struct vec2 b) {
    return (struct vec2) { { max_real(a._[0], b._[0]), max_real(a._[1], b._[1]) } };
}

static inline struct vec2 scale_vec2(struct vec2 a, real_t f) {
    return (struct vec2) { { a._[0] * f, a._[1] * f } };
}

static inline real_t dot_vec2(struct vec2 a, struct vec2 b) {
    return a._[0] * b._[0] + a._[1] * b._[1];
}

#endif
