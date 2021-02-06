#ifndef SCENE_SCENE_H
#define SCENE_SCENE_H

#include "bvh/bvh.h"

struct tri;
struct camera;
struct mem_pool;
struct thread_pool;

struct scene {
    struct mem_pool* mem_pool;
    struct camera* camera;
    struct tri* tris;
    size_t tri_count;
    struct bvh bvh;
};

// Loads a scene from a file on disk. Uses the thread pool to parallelize tasks.
struct scene* load_scene(struct thread_pool* thread_pool, const char* file_name);

void free_scene(struct scene*);

/* Intersects a ray with the scene.
 * Returns when closest intersection is found if `any == false`, or
 * when any intersection is found if `any == true`.
 */
bool intersect_ray_scene(struct ray* ray, const struct scene* scene, struct hit* hit, bool any);

#endif
