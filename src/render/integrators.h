#ifndef RENDER_INTEGRATORS_H
#define RENDER_INTEGRATORS_H

#include "scene/scene.h"

struct render_target {
    // Viewport coordinates onto the image to render on.
    size_t x_min, y_min;
    size_t x_max, y_max;
    size_t frame_index; // Current frame index, starting at 0
    struct image* image;
};

void render_debug(
    struct thread_pool* thread_pool,
    struct render_target* render_target,
    const struct scene*);
    
#endif
