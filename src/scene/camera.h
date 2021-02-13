#ifndef SCENE_CAMERA_H
#define SCENE_CAMERA_H

#include "scene/scene.h"
#include "core/ray.h"
#include "core/utils.h"
#include "core/vec2.h"

/*
 * Cameras are special scene nodes in the sense that they can
 * be modified (by user input). This means that each camera is
 * a new object, regardless of whether a similar camera exists or not.
 */

// User event that can control the camera.
// Different cameras can respond differently to the same event.
struct camera_event {
    real_t mouse_move[2]; // Relative mouse movement on X and Y

    // Relative keyboard movement generated from the
    // UP, DOWN, LEFT, RIGHT keyboard keys.
    real_t keyboard_move[4];
};

struct camera {
    struct scene_node node;

    // Generates a ray for a position `(x, y)` in `[-1, 1]^2`.
    // This function returns a ray whose direction is normalized.
    struct ray (*generate_ray)(const struct camera*, const struct vec2*);
    // Updates a camera after a mouse movement or a keypress.
    void (*update)(struct camera*, const struct camera_event*);
};

/*
 * Converts screen coordinates to camera coordinates.
 * The offset is a 2D vector in `[0, 1]^2` that represents
 * the offset within a pixel: `(0, 0)` is the top-left corner of that pixel,
 * and `(1, 1)` is the top-right one.
 */
static inline struct vec2 image_to_camera(
    size_t x, size_t y,
    size_t w, size_t h,
    const struct vec2* offset)
{
    const real_t inv_x = (real_t) 2 / (real_t)w;
    const real_t inv_y = (real_t)-2 / (real_t)h;
    return (struct vec2) {
        {
            fast_mul_add(x + offset->_[0], inv_x, (real_t)-1),
            fast_mul_add(y + offset->_[1], inv_y, (real_t) 1)
        }
    };
}

struct camera* new_perspective_camera(
    struct scene* scene,
    const struct vec3* eye,
    const struct vec3* dir,
    const struct vec3* up,
    real_t fov, real_t ratio);

#endif
