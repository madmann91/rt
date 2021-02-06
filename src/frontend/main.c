#include <stdlib.h>
#include <stdio.h>

#include "render/integrators.h"
#include "scene/scene.h"
#include "scene/camera.h"
#include "core/thread_pool.h"
#include "core/image.h"
#include "io/png_image.h"

static inline void usage(void) {
    fprintf(stderr,
        "rt -- A fast and minimalistic renderer\n"
        "\n"
        "rt uses a configuration file to specify the scene to render.\n"
        "Try running `rt file.toml' where `file.toml' is a valid scene file.\n"
        "See https://github.com/madmann91/rt for more information.\n");
}

int main(int argc, char** argv) {
    size_t width = 1080, height = 720;
    if (argc < 2) {
        usage();
        return EXIT_FAILURE;
    }

    struct thread_pool* thread_pool = NULL;
    struct scene* scene = NULL;
    struct image* image = NULL;
    int status = EXIT_SUCCESS;

    thread_pool = new_thread_pool(detect_system_thread_count());
    scene = load_scene(thread_pool, argv[1]);
    if (!scene) {
        status = EXIT_FAILURE;
        goto cleanup;
    }
    image = new_rgb_image(width, height);
    scene->camera = new_perspective_camera(scene,
        &(struct vec3) { { -4, 1.3, 0.0 } },
        &(struct vec3) { { 1, -0.1, 0 } },
        &(struct vec3) { { 0, 1, 0 } },
        48,
        (real_t)width / (real_t)height);

    render_debug(thread_pool, scene, &(struct render_target) {
        .x_min = 0, .x_max = image->width,
        .y_min = 0, .y_max = image->height,
        .image = image
    });

    save_png_image("render.png", image);

cleanup:
    if (thread_pool) free_thread_pool(thread_pool);
    if (scene) free_scene(scene);
    if (image) free_image(image);
    return status;
}
