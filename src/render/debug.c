#include "render/integrators.h"
#include "scene/camera.h"
#include "core/thread_pool.h"
#include "core/random.h"
#include "core/image.h"
#include "bvh/tri.h"

struct tile_task {
    struct parallel_task_2d task;
    const struct scene* scene;
    struct render_target* render_target;
};

static void run_tile_task(struct parallel_task_2d* task, size_t thread_id) {
    IGNORE(thread_id);
    struct tile_task* tile_task = (void*)task;
    const struct scene* scene = tile_task->scene;
    struct render_target* render_target = tile_task->render_target;
    struct image* target_image = render_target->image;

    uint64_t seed = random_seed(task->range[0].begin, task->range[1].begin, render_target->frame_index);
    struct rnd_gen rnd_gen = make_rnd_gen(seed);
    for (size_t i = task->range[1].begin, n = task->range[1].end; i < n; ++i) {
        for (size_t j = task->range[0].begin, m = task->range[0].end; j < m; ++j) {
            struct vec2 offset = random_vec2_01(&rnd_gen);
            struct vec2 xy = image_to_camera(j, i, target_image->width, target_image->height, &offset);
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
    parallel_for_2d(
        thread_pool,
        run_tile_task,
        (struct parallel_task_2d*)&(struct tile_task) {
            .scene = scene,
            .render_target = render_target
        },
        sizeof(struct tile_task),
        (struct range[2]) {
            { render_target->x_min, render_target->x_max },
            { render_target->y_min, render_target->y_max }
        });
}
