#include "scene/scene.h"

void free_scene(struct scene* scene) {
    free(scene->tris);
    free(scene);
}
