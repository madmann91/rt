#include "render/render.h"
#include "scene/camera.h"
#include "core/thread_pool.h"
#include "core/random.h"
#include "scene/image.h"

struct tile_task {
    struct parallel_task_2d task;
    const struct render_params* render_params;
};

static void run_tile_task(struct parallel_task_2d* task, size_t thread_id) {
    IGNORE(thread_id);

    struct tile_task* tile_task = (void*)task;
    const struct render_params* render_params = tile_task->render_params;
    struct image* target_image = render_params->target_image;
    geometry_t geometry = render_params->geometry;
    struct camera* camera = render_params->camera;

    uint64_t seed = random_seed(task->range[0].begin, task->range[1].begin, render_params->frame_index);
    struct rnd_gen rnd_gen = make_rnd_gen(seed);
    for (size_t i = task->range[1].begin, n = task->range[1].end; i < n; ++i) {
        for (size_t j = task->range[0].begin, m = task->range[0].end; j < m; ++j) {
            struct vec2 offset = random_vec2_01(&rnd_gen);
            struct vec2 xy = image_to_camera(j, i, target_image->width, target_image->height, &offset);
            struct ray ray = camera->generate_ray(camera, &xy);
            struct hit hit = empty_hit();

            struct rgb color = black;
            if (intersect_ray_geometry(&ray, &hit, geometry, false)) {
                struct vec3 normal = normalize_vec3(get_geometry_attr(geometry, ATTR_SHADING_NORMAL, &ray, &hit).vec3);
                real_t intensity = fabs(dot_vec3(normal, ray.dir));
                color = gray(intensity);
            }
            set_rgb_pixel(target_image, j, i, &color);
        }
    }
}

static void render_debug(
    struct thread_pool* thread_pool,
    const struct render_params* render_params)
{
    parallel_for_2d(
        thread_pool,
        run_tile_task,
        (struct parallel_task_2d*)&(struct tile_task) {
            .render_params = render_params
        },
        sizeof(struct tile_task),
        (struct range[2]) {
            { render_params->viewport.x_min, render_params->viewport.x_max },
            { render_params->viewport.y_min, render_params->viewport.y_max }
        });
}

render_fn_t render_debug_fn = render_debug;
