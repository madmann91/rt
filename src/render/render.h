#ifndef RENDER_RENDER_H
#define RENDER_RENDER_H

#include "scene/scene.h"
#include "scene/geometry.h"
#include "scene/camera.h"

struct render_params {
    struct {
        size_t x_min, x_max;
        size_t y_min, y_max;
    } viewport;
    size_t frame_index;
    struct image* target_image;
    struct scene* scene;
    geometry_t geometry;
    struct camera* camera;
};

typedef void (*render_fn_t)(struct thread_pool*, const struct render_params*);

extern render_fn_t render_debug_fn;

#endif
