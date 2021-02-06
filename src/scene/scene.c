#include <stdio.h>

#include "scene/scene.h"
#include "core/thread_pool.h"
#include "core/mem_pool.h"
#include "core/parallel.h"
#include "loaders/obj.h"
#include "bvh/bvh.h"
#include "bvh/tri.h"

static inline size_t count_triangles(struct obj* obj) {
    size_t tri_count = 0;
    for (size_t i = 0; i < obj->face_count; ++i)
        tri_count += obj->faces[i].index_count - 2;
    return tri_count;
}

static inline void fill_tris(struct tri* tris, struct obj* obj) {
    size_t tri_count = 0;
    for (size_t i = 0; i < obj->face_count; ++i) {
        struct obj_face* face = &obj->faces[i];
        struct vec3 v0 = obj->vertices[obj->indices[face->first_index + 0].v - 1];
        struct vec3 v1 = obj->vertices[obj->indices[face->first_index + 1].v - 1];
        for (size_t j = 2; j < face->index_count; ++j) {
            struct vec3 v2 = obj->vertices[obj->indices[face->first_index + j].v - 1];
            tris[tri_count++] = make_tri(v0, v1, v2);
            v1 = v2;
        }
    }
}

struct permute_task {
    struct parallel_task task;
    const struct tri* src_tris;
    struct tri* dst_tris;
    const size_t* primitive_indices;
};

static void run_permute_task(struct parallel_task* task) {
    struct permute_task* permute_task = (void*)task;
    for (size_t i = task->begin[0], n = task->end[0]; i < n; ++i)
        permute_task->dst_tris[permute_task->primitive_indices[i]] = permute_task->src_tris[i];
}

SWAP(tris, struct tri*)

static inline void permute_tris(
    struct thread_pool* thread_pool,
    struct tri** tris,
    const size_t* primitive_indices,
    size_t tri_count)
{
    struct tri* dst_tris = xmalloc(sizeof(struct tri) * tri_count);
    parallel_for(
        thread_pool,
        run_permute_task,
        (struct parallel_task*)&(struct permute_task) {
            .src_tris = *tris,
            .dst_tris = dst_tris,
            .primitive_indices = primitive_indices
        },
        sizeof(struct permute_task),
        (size_t[3]) { 0 },
        (size_t[3]) { tri_count, 1, 1 });
    swap_tris(tris, &dst_tris);
}

static inline struct bbox get_tri_bbox(void* primitive_data, size_t index) {
    const struct tri* tri = &((struct tri*)primitive_data)[index];
    return extend_bbox(extend_bbox(point_bbox(tri->p0), get_tri_p1(tri)), get_tri_p2(tri));
}

static inline struct vec3 get_tri_center(void* primitive_data, size_t index) {
    const struct tri* tri = &((struct tri*)primitive_data)[index];
    return scale_vec3(add_vec3(tri->p0, add_vec3(get_tri_p1(tri), get_tri_p2(tri))), (real_t)1 / (real_t)3);
}

static inline struct bvh build_tri_bvh(struct thread_pool* thread_pool, struct tri** tris, size_t tri_count) {
    struct bvh bvh = build_bvh(thread_pool, *tris, get_tri_bbox, get_tri_center, tri_count, 1.5);
    permute_tris(thread_pool, tris, bvh.primitive_indices, tri_count);
    return bvh;
}

struct scene* load_scene(struct thread_pool* thread_pool, const char* file_name) {
    struct obj* obj = load_obj(file_name);
    if (!obj)
        return NULL;
    if (obj->face_count == 0) {
        free_obj(obj);
        return NULL;
    }

    printf("Loading scene file '%s'\n", file_name);
    struct scene* scene = xmalloc(sizeof(struct scene));
    scene->mem_pool = new_mem_pool();
    scene->tri_count = count_triangles(obj);
    printf("- Found %zu triangles\n", scene->tri_count);
    scene->tris = xmalloc(sizeof(struct tri) * scene->tri_count);
    fill_tris(scene->tris, obj);
    free_obj(obj);

    struct timespec t_start;
    timespec_get(&t_start, TIME_UTC);
    scene->bvh = build_tri_bvh(thread_pool, &scene->tris, scene->tri_count);
    struct timespec t_end;
    timespec_get(&t_end, TIME_UTC);

    printf("- Building BVH took %gms (%zu node(s))\n", elapsed_seconds(&t_start, &t_end) * 1.e3, scene->bvh.node_count);
    return scene;
}

void free_scene(struct scene* scene) {
    free_bvh(&scene->bvh);
    free_mem_pool(scene->mem_pool);
    free(scene->tris);
    free(scene);
}

static bool intersect_bvh_leaf_tris(
    void* intersection_data,
    const struct bvh_node* leaf,
    struct ray* ray, struct hit* hit,
    bool any)
{
    const struct tri* tris = intersection_data;
    bool found = false;
    for (size_t i = 0, j = leaf->first_child_or_primitive, n = leaf->primitive_count; i < n; ++i, ++j) {
        found |= intersect_ray_tri(ray, &tris[j], hit);
        if (any && found)
            return true;
    }
    return found;
}

static bool intersect_bvh_leaf_tris_any(
    void* intersection_data,
    const struct bvh_node* leaf,
    struct ray* ray, struct hit* hit)
{
    return intersect_bvh_leaf_tris(intersection_data, leaf, ray, hit, true);
}

static bool intersect_bvh_leaf_tris_closest(
    void* intersection_data,
    const struct bvh_node* leaf,
    struct ray* ray, struct hit* hit)
{
    return intersect_bvh_leaf_tris(intersection_data, leaf, ray, hit, false);
}

bool intersect_ray_scene(struct ray* ray, const struct scene* scene, struct hit* hit, bool any) {
    hit->primitive_index = INVALID_PRIMITIVE_INDEX;
    intersect_bvh(
        scene->tris,
        any ? intersect_bvh_leaf_tris_any : intersect_bvh_leaf_tris_closest,
        &scene->bvh, ray, hit, any);
    return hit->primitive_index != INVALID_PRIMITIVE_INDEX;
}
