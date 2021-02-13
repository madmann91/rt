#ifndef CORE_VEC4_H
#define CORE_VEC4_H

#include "core/vec3.h"

struct vec4 { real_t _[4]; };

static inline struct vec4 const_vec4(real_t x) {
    return (struct vec4) { { x, x, x, x } };
}

static inline struct vec4 make_vec4(real_t x, real_t y, real_t z, real_t w) {
    return (struct vec4) { { x, y, z, w } };
}

static inline struct vec4 vec2_to_vec4(struct vec2 a, real_t z, real_t w) {
    return make_vec4(a._[0], a._[1], z, w);
}

static inline struct vec4 vec3_to_vec4(struct vec3 a, real_t w) {
    return make_vec4(a._[0], a._[1], a._[2], w);
}

static inline struct vec2 vec4_to_vec2(struct vec4 a) {
    return make_vec2(a._[0], a._[1]);
}

static inline struct vec3 vec4_to_vec3(struct vec4 a) {
    return make_vec3(a._[0], a._[1], a._[2]);
}

static inline struct vec4 add_vec4(struct vec4 a, struct vec4 b) {
    return make_vec4(a._[0] + b._[0], a._[1] + b._[1], a._[2] + b._[2], a._[3] + b._[3]);
}

static inline struct vec4 sub_vec4(struct vec4 a, struct vec4 b) {
    return make_vec4(a._[0] - b._[0], a._[1] - b._[1], a._[2] - b._[2], a._[3] - b._[3]);
}

static inline struct vec4 mul_vec4(struct vec4 a, struct vec4 b) {
    return make_vec4(a._[0] * b._[0], a._[1] * b._[1], a._[2] * b._[2], a._[3] * b._[3]);
}

static inline struct vec4 div_vec4(struct vec4 a, struct vec4 b) {
    return make_vec4(a._[0] / b._[0], a._[1] / b._[1], a._[2] / b._[2], a._[3] / b._[3]);
}

static inline struct vec4 min_vec4(struct vec4 a, struct vec4 b) {
    return make_vec4(
        min_real(a._[0], b._[0]),
        min_real(a._[1], b._[1]),
        min_real(a._[2], b._[2]),
        min_real(a._[3], b._[3]));
}

static inline struct vec4 max_vec4(struct vec4 a, struct vec4 b) {
    return make_vec4(
        max_real(a._[0], b._[0]),
        max_real(a._[1], b._[1]),
        max_real(a._[2], b._[2]),
        max_real(a._[3], b._[3]));
}

static inline struct vec4 lerp3_vec4(
    struct vec4 x, struct vec4 y, struct vec4 z, real_t u, real_t v)
{
    return make_vec4(
        lerp3_real(x._[0], y._[0], z._[0], u, v),
        lerp3_real(x._[1], y._[1], z._[1], u, v),
        lerp3_real(x._[2], y._[2], z._[2], u, v),
        lerp3_real(x._[3], y._[3], z._[3], u, v));
}

static inline struct vec4 lerp4_vec4(
    struct vec4 x, struct vec4 y, struct vec4 z, struct vec4 w, real_t u, real_t v)
{
    return make_vec4(
        lerp4_real(x._[0], y._[0], z._[0], w._[0], u, v),
        lerp4_real(x._[1], y._[1], z._[1], w._[1], u, v),
        lerp4_real(x._[2], y._[2], z._[2], w._[2], u, v),
        lerp4_real(x._[3], y._[3], z._[3], w._[3], u, v));
}

static inline struct vec4 scale_vec4(struct vec4 a, real_t f) {
    return make_vec4(a._[0] * f, a._[1] * f, a._[2] * f, a._[3] * f);
}

static inline struct vec4 neg_vec4(struct vec4 a) {
    return make_vec4(-a._[0], -a._[1], -a._[2], -a._[3]);
}

static inline real_t dot_vec4(struct vec4 a, struct vec4 b) {
    return
        fast_mul_add(a._[0], b._[0],
        fast_mul_add(a._[1], b._[1],
        fast_mul_add(a._[2], b._[2],
            a._[3] * b._[3])));
}

static inline real_t lensq_vec4(struct vec4 a) {
    return dot_vec4(a, a);
}

static inline real_t len_vec4(struct vec4 a) {
    return sqrt(lensq_vec4(a));
}

static inline struct vec4 normalize_vec4(struct vec4 a) {
    return scale_vec4(a, ((real_t)1) / len_vec4(a));
}

#endif
