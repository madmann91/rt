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

static struct ray generate_perspective_ray(const struct camera* camera, const struct vec2* xy) {
    const struct perspective_camera* perspective_camera = (void*)camera;
    const struct vec3* dir = &perspective_camera->dir;
    const struct vec3* right = &perspective_camera->right;
    const struct vec3* up = &perspective_camera->up;
    struct vec3 ray_dir = {
        {
            fast_mul_add(xy->_[0], right->_[0], fast_mul_add(xy->_[1], up->_[0], dir->_[0])),
            fast_mul_add(xy->_[0], right->_[1], fast_mul_add(xy->_[1], up->_[1], dir->_[1])),
            fast_mul_add(xy->_[0], right->_[2], fast_mul_add(xy->_[1], up->_[2], dir->_[2]))
        }
    };
    return (struct ray) {
        .org = perspective_camera->eye,
        .dir = normalize_vec3(ray_dir),
        .t_min = 0, .t_max = REAL_MAX
    };
}

static void update_perspective_camera(struct camera* camera, const struct camera_event* event) {
    // TODO
    IGNORE(camera);
    IGNORE(event);
}

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
    camera->right = normalize_vec3(cross_vec3(*dir, *up));
    camera->up    = normalize_vec3(cross_vec3(camera->right, *dir));

    real_t width = tan(fov * REAL_PI / (real_t)360);
    real_t height = width / ratio;
    camera->right = scale_vec3(camera->right, width);
    camera->up    = scale_vec3(camera->up, height);

    camera->camera.generate_ray = generate_perspective_ray;
    camera->camera.update       = update_perspective_camera;
    return &camera->camera;
}
