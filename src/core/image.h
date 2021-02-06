#ifndef CORE_IMAGE_H
#define CORE_IMAGE_H

#include <stddef.h>
#include <assert.h>

#include "core/rgb.h"
#include "core/alloc.h"

struct image {
    size_t width, height;
    struct rgb pixels[];
};

static inline struct image* new_image(size_t width, size_t height) {
    struct image* image = xmalloc(sizeof(struct image) + sizeof(struct rgb) * width * height);
    image->width = width;
    image->height = height;
    return image;
}

static inline void free_image(struct image* image) {
    free(image);
}

static inline struct rgb* pixel_at(struct image* image, size_t x, size_t y) {
    assert(x < image->width && y < image->height);
    return &image->pixels[y * image->width + x];
}

#endif
