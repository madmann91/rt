#include "scene/camera.h"
#include "scene/scene.h"
#include "core/hash.h"

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

static uint32_t hash_camera(const struct scene_node* node) {
    return hash_ptr(hash_init(), node);
}

static bool compare_camera(const struct scene_node* left, const struct scene_node* right) {
    return left == right;
}

struct camera* new_perspective_camera(
    struct scene* scene,
    const struct vec3* eye,
    const struct vec3* dir,
    const struct vec3* up,
    real_t fov, real_t ratio)
{
    real_t width = tan(fov * REAL_PI / (real_t)360);
    real_t height = width / ratio;
    struct vec3 right = cross_vec3(*dir, *up);
    struct perspective_camera perspective_camera = { 
        .camera = {
            .node = {
                .hash    = hash_camera,
                .compare = compare_camera
            },
            .generate_ray = generate_perspective_ray,
            .update       = update_perspective_camera
        },
        .eye   = *eye,
        .dir   = normalize_vec3(*dir),
        .right = scale_vec3(normalize_vec3(right), width),
        .up    = scale_vec3(normalize_vec3(cross_vec3(right, *dir)), height)
    };
    return (struct camera*)insert_scene_node(scene,
        &perspective_camera.camera.node,
        sizeof(struct perspective_camera));
}
