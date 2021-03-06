#ifndef CORE_VEC2_H
#define CORE_VEC2_H

#include "core/config.h"
#include "core/utils.h"

struct vec2 { real_t _[2]; };

static inline struct vec2 const_vec2(real_t x) {
    return (struct vec2) { { x, x } };
}

static inline struct vec2 make_vec2(real_t x, real_t y) {
    return (struct vec2) { { x, y } };
}

static inline struct vec2 add_vec2(struct vec2 a, struct vec2 b) {
    return make_vec2(a._[0] + b._[0], a._[1] + b._[1]);
}

static inline struct vec2 sub_vec2(struct vec2 a, struct vec2 b) {
    return make_vec2(a._[0] - b._[0], a._[1] - b._[1]);
}

static inline struct vec2 mul_vec2(struct vec2 a, struct vec2 b) {
    return make_vec2(a._[0] * b._[0], a._[1] * b._[1]);
}

static inline struct vec2 div_vec2(struct vec2 a, struct vec2 b) {
    return make_vec2(a._[0] / b._[0], a._[1] / b._[1]);
}

static inline struct vec2 min_vec2(struct vec2 a, struct vec2 b) {
    return make_vec2(min_real(a._[0], b._[0]), min_real(a._[1], b._[1]));
}

static inline struct vec2 max_vec2(struct vec2 a, struct vec2 b) {
    return make_vec2(max_real(a._[0], b._[0]), max_real(a._[1], b._[1]));
}

static inline struct vec2 lerp3_vec2(
    struct vec2 x, struct vec2 y, struct vec2 z, real_t u, real_t v)
{
    return make_vec2(
        lerp3_real(x._[0], y._[0], z._[0], u, v),
        lerp3_real(x._[1], y._[1], z._[1], u, v));
}

static inline struct vec2 lerp4_vec2(
    struct vec2 x, struct vec2 y, struct vec2 z, struct vec2 w, real_t u, real_t v)
{
    return make_vec2(
        lerp4_real(x._[0], y._[0], z._[0], w._[0], u, v),
        lerp4_real(x._[1], y._[1], z._[1], w._[1], u, v));
}

static inline struct vec2 scale_vec2(struct vec2 a, real_t f) {
    return make_vec2(a._[0] * f, a._[1] * f);
}

static inline struct vec2 neg_vec2(struct vec2 a) {
    return make_vec2(-a._[0], -a._[1]);
}

static inline real_t dot_vec2(struct vec2 a, struct vec2 b) {
    return fast_mul_add(a._[0], b._[0], a._[1] * b._[1]);
}

static inline real_t lensq_vec2(struct vec2 a) {
    return dot_vec2(a, a);
}

static inline real_t len_vec2(struct vec2 a) {
    return sqrt(lensq_vec2(a));
}

static inline struct vec2 normalize_vec2(struct vec2 a) {
    return scale_vec2(a, ((real_t)1) / len_vec2(a));
}

#endif
