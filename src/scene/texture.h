#ifndef SCENE_TEXTURE_H
#define SCENE_TEXTURE_H

#include "scene/scene.h"
#include "scene/geometry.h"
#include "core/ray.h"
#include "core/vec2.h"

/*
 * Textures can either be varying on the surface of an object,
 * or constant. They are paired with a sampler object that
 * describes how to sample them. 
 */

struct image;

typedef const struct texture* texture_t;
typedef const struct coord_mapper* coord_mapper_t;

struct texture {
    struct rgb (*evaluate)(texture_t);
};

struct coord_mapper {
    struct vec2 (*get_tex_coords)(
        coord_mapper_t,
        const struct ray*,
        const struct hit*,
        geometry_t);
};

enum image_filter {
    FILTER_NEAREST,
    FILTER_BILINEAR
};

enum border_handling {
    CLAMP_BORDER,
    MIRROR_BORDER,
    REPEAT_BORDER
};

texture_t new_constant_texture(struct scene*, struct rgb);
texture_t new_image_texture(struct scene*, struct image*, enum image_filter, enum border_handling);

#endif
