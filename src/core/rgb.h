#ifndef CORE_RGB_H
#define CORE_RGB_H

#include "core/config.h"

struct rgb {
    real_t r, g, b;
};

static inline struct rgb add_rgb(struct rgb a, struct rgb b) {
    return (struct rgb) { .r = a.r + b.r, .g = a.g + b.g, .b = a.b + b.b };
}

static inline struct rgb sub_rgb(struct rgb a, struct rgb b) {
    return (struct rgb) { .r = a.r - b.r, .g = a.g - b.g, .b = a.b - b.b };
}

static inline struct rgb mul_rgb(struct rgb a, struct rgb b) {
    return (struct rgb) { .r = a.r * b.r, .g = a.g * b.g, .b = a.b * b.b };
}

static inline struct rgb scale_rgb(struct rgb a, real_t f) {
    return (struct rgb) { .r = a.r * f, .g = a.g * f, .b = a.b * f };
}

static inline struct rgb gray(real_t intensity) {
    return (struct rgb) { .r = intensity, .g = intensity, .b = intensity };
}

static const struct rgb black = { 0 };
static const struct rgb white = { 1, 1, 1 };

#endif
