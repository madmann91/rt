#include "scene/camera.h"
#include "scene/scene.h"
#include "core/mem_pool.h"

struct perspective_camera {
    struct camera camera;
    struct vec3 eye;
    struct vec3 dir;
    struct vec3 right;
    struct vec3 up;
};

struct camera* new_perspective_camera(
    struct scene* scene,
    const struct vec3* eye,
    const struct vec3* dir,
    const struct vec3* up,
    real_t fov, real_t ratio)
{
    struct perspective_camera* camera = alloc_from_pool(
        &scene->mem_pool, sizeof(struct perspective_camera));
    camera->eye   = *eye;
    camera->dir   = normalize_vec3(*dir);
    camera->right = normalize_vec3(cross_vec3(camera->dir, *up));
    camera->up    = normalize_vec3(cross_vec3(camera->right, camera->dir));

    real_t width = tan(fov * REAL_PI / (real_t)360), height = width / ratio;
    camera->right = scale_vec3(camera->right, width);
    camera->up    = scale_vec3(camera->up, height);
    return &camera->camera;
}
