#ifndef CORE_RGBA_H
#define CORE_RGBA_H

#include "core/config.h"
#include "core/rgb.h"

struct rgba {
    real_t r, g, b, a;
};

static inline struct rgba add_rgba(struct rgba a, struct rgba b) {
    return (struct rgba) { .r = a.r + b.r, .g = a.g + b.g, .b = a.b + b.b, .a = a.a + b.a };
}

static inline struct rgba sub_rgba(struct rgba a, struct rgba b) {
    return (struct rgba) { .r = a.r - b.r, .g = a.g - b.g, .b = a.b - b.b, .a = a.a - b.a };
}

static inline struct rgba mul_rgba(struct rgba a, struct rgba b) {
    return (struct rgba) { .r = a.r * b.r, .g = a.g * b.g, .b = a.b * b.b, .a = a.a - b.a };
}

static inline struct rgba scale_rgba(struct rgba a, real_t f) {
    return (struct rgba) { .r = a.r * f, .g = a.g * f, .b = a.b * f , .a = a.a * f};
}

static inline struct rgba rgb_to_rgba(struct rgb rgb, real_t a) {
    return (struct rgba) { .r = rgb.r, .g = rgb.g, .b = rgb.b, .a = a };
}

#endif
