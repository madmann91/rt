#ifndef SCENE_SCENE_H
#define SCENE_SCENE_H

#include "bvh/tri.h"
#include "bvh/bvh.h"

struct scene {
    struct tri* tris;
    size_t tri_count;
    struct bvh bvh;
};

/* Loads a scene from a file on disk. */
struct scene* load_scene(const char* file_name);
void free_scene(struct scene*);

/* Intersects a ray with the scene.
 * Returns when closest intersection is found if `any == false`, or
 * when any intersection is found if `any == true`.
 */
bool intersect_ray_scene(struct ray* ray, const struct scene* scene, struct hit* hit, bool any);

#endif
