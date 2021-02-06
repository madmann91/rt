#ifndef CORE_IMAGE_H
#define CORE_IMAGE_H

#include <stddef.h>
#include <assert.h>

#include "core/alloc.h"
#include "core/rgba.h"
#include "core/rgb.h"

enum {
    R_CHANNEL_INDEX,
    G_CHANNEL_INDEX,
    B_CHANNEL_INDEX,
    A_CHANNEL_INDEX
};

struct image {
    size_t width, height;
    size_t channel_count;
    real_t* channels[];
};

static inline struct image* new_image(size_t width, size_t height, size_t channel_count) {
    struct image* image = xmalloc(sizeof(struct image) + sizeof(real_t*) * channel_count);
    image->width = width;
    image->height = height;
    image->channel_count = channel_count;
    for (size_t i = 0; i < channel_count; ++i)
        image->channels[i] = xmalloc(sizeof(real_t) * width * height);
    return image;
}

static inline struct image* new_rgb_image(size_t width, size_t height) {
    return new_image(width, height, 3);
}

static inline struct image* new_rgba_image(size_t width, size_t height) {
    return new_image(width, height, 4);
}

static inline bool is_rgb_image(const struct image* image) {
    return image->channel_count == 3;
}

static inline bool is_rgba_image(const struct image* image) {
    return image->channel_count == 4;
}

static inline bool is_rgb_or_rgba_image(const struct image* image) {
    return is_rgb_image(image) || is_rgba_image(image);
}

static inline void free_image(struct image* image) {
    for (size_t i = 0, n = image->channel_count; i < n; ++i)
        free(image->channels[i]);
    free(image);
}

static inline struct rgb get_rgb_pixel(const struct image* image, size_t x, size_t y) {
    assert(x < image->width && y < image->height && image->channel_count >= 3);
    return (struct rgb) {
        .r = image->channels[R_CHANNEL_INDEX][y * image->width + x],
        .g = image->channels[G_CHANNEL_INDEX][y * image->width + x],
        .b = image->channels[B_CHANNEL_INDEX][y * image->width + x]
    };
}

static inline struct rgba get_rgba_pixel(const struct image* image, size_t x, size_t y) {
    assert(x < image->width && y < image->height && image->channel_count >= 4);
    return (struct rgba) {
        .r = image->channels[R_CHANNEL_INDEX][y * image->width + x],
        .g = image->channels[G_CHANNEL_INDEX][y * image->width + x],
        .b = image->channels[B_CHANNEL_INDEX][y * image->width + x],
        .a = image->channels[A_CHANNEL_INDEX][y * image->width + x]
    };
}

static inline void set_rgb_pixel(struct image* image, size_t x, size_t y, const struct rgb* pixel) {
    assert(x < image->width && y < image->height && image->channel_count >= 3);
    image->channels[R_CHANNEL_INDEX][y * image->width + x] = pixel->r;
    image->channels[G_CHANNEL_INDEX][y * image->width + x] = pixel->g;
    image->channels[B_CHANNEL_INDEX][y * image->width + x] = pixel->b;
}

static inline void set_rgba_pixel(struct image* image, size_t x, size_t y, const struct rgba* pixel) {
    assert(x < image->width && y < image->height && image->channel_count >= 4);
    image->channels[R_CHANNEL_INDEX][y * image->width + x] = pixel->r;
    image->channels[G_CHANNEL_INDEX][y * image->width + x] = pixel->g;
    image->channels[B_CHANNEL_INDEX][y * image->width + x] = pixel->b;
    image->channels[A_CHANNEL_INDEX][y * image->width + x] = pixel->a;
}

#endif
