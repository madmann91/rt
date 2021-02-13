#include <stdlib.h>
#include <stdio.h>

#include "scene/scene.h"
#include "scene/camera.h"
#include "scene/geometry.h"
#include "scene/image.h"
#include "core/thread_pool.h"
#include "io/import_obj.h"
#include "io/png_image.h"
#include "render/render.h"

static inline void usage(void) {
    fprintf(stderr, "rt -- A fast and minimalistic renderer\n");
}

int main(int argc, char** argv) {
    size_t width = 1080, height = 720;
    if (argc < 2) {
        usage();
        return EXIT_FAILURE;
    }

    struct thread_pool* thread_pool = NULL;
    struct scene* scene = NULL;
    struct camera* camera = NULL;
    struct image* image = NULL;
    struct mesh* mesh = NULL;
    geometry_t geometry;
    int status = EXIT_SUCCESS;

    thread_pool = new_thread_pool(detect_system_thread_count());
    scene = new_scene();
    image = new_rgb_image(width, height);
    camera = new_perspective_camera(
        scene,
#if 0
        // Dining room
        &(struct vec3) { { -4, 1.3, 0.0 } },
        &(struct vec3) { { 1, -0.1, 0 } },
        &(struct vec3) { { 0, 1, 0 } },
        48,
#else
        // Cornell box
        &(struct vec3) { { 0, 0.9, 2.5 } },
        &(struct vec3) { { 0, 0, -1 } },
        &(struct vec3) { { 0, 1, 0 } },
        60,
#endif
        (real_t)width / (real_t)height);

    mesh = import_obj_model(scene, argv[1]);
    if (!mesh) {
        fprintf(stderr, "Cannot load OBJ model");
        goto cleanup;
    }
    geometry = new_mesh_geometry(scene, mesh);
    prepare_geometry(geometry, thread_pool);

    render_debug_fn(thread_pool, &(struct render_params) {
        .viewport = {
            .x_min = 0, .x_max = image->width,
            .y_min = 0, .y_max = image->height
        },
        .scene = scene,
        .camera = camera,
        .geometry = geometry,
        .target_image = image
    });

    save_png_image("render.png", image);

cleanup:
    if (thread_pool) free_thread_pool(thread_pool);
    if (scene) free_scene(scene);
    if (mesh) free_mesh(mesh);
    if (image) free_image(image);
    return status;
}
