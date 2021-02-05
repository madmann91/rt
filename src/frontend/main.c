#include <stdlib.h>
#include <stdio.h>

#include "scene/scene.h"

static inline void usage(void) {
    fprintf(stderr,
        "rt -- A fast and minimalistic renderer\n"
        "\n"
        "rt uses a configuration file to specify the scene to render.\n"
        "Try running `rt file.toml' where `file.toml' is a valid scene file.\n"
        "See https://github.com/madmann91/rt for more information.\n");
}

int main(int argc, char** argv) {
    if (argc < 2)
        return EXIT_FAILURE;
    struct scene* scene = load_scene(argv[1]);
    if (!scene)
        return EXIT_FAILURE;
    free_scene(scene);
    return 0;
}
