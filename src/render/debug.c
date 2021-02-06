#include "render/integrators.h"
#include "scene/camera.h"
#include "core/parallel.h"
#include "core/random.h"
#include "core/image.h"
#include "bvh/tri.h"

struct tile_task {
    struct parallel_task task;
    const struct scene* scene;
    struct render_target* render_target;
};

static void run_tile_task(struct parallel_task* task) {
    struct tile_task* tile_task = (void*)task;
    const struct scene* scene = tile_task->scene;
    struct render_target* render_target = tile_task->render_target;
    struct image* target_image = render_target->image;

    uint64_t seed = random_seed(task->begin[0], task->begin[1], render_target->frame_index);
    struct rnd_gen rnd_gen = make_rnd_gen(seed);
    for (size_t i = task->begin[1], n = task->end[1]; i < n; ++i) {
        for (size_t j = task->begin[0], m = task->end[0]; j < m; ++j) {
            struct vec2 offset = random_vec2_01(&rnd_gen);
            struct vec2 xy = screen_to_camera(j, i, target_image->width, target_image->height, &offset);
            struct ray ray = scene->camera->generate_ray(scene->camera, &xy);
            struct hit hit;

            struct rgb color = { 0, 0, 0 };
            if (intersect_ray_scene(&ray, scene, &hit, false)) {
                // TODO: Interpolate vertex normals
                struct vec3 normal = normalize_vec3(scene->tris[hit.primitive_index].n);
                real_t intensity = fabs(dot_vec3(normal, ray.dir));
                color = gray(intensity);
            }
            set_rgb_pixel(target_image, j, i, &color);
        }
    }
}

void render_debug(
    struct thread_pool* thread_pool,
    const struct scene* scene,
    struct render_target* render_target)
{
    assert(scene->camera);
    parallel_for(
        thread_pool,
        run_tile_task,
        (struct parallel_task*)&(struct tile_task) {
            .scene = scene,
            .render_target = render_target
        },
        sizeof(struct tile_task),
        (size_t[3]) { render_target->x_min, render_target->y_min, 0 },
        (size_t[3]) { render_target->x_max, render_target->y_max, 1 });
}
