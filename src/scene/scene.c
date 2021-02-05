#include <stdio.h>

#include "scene/scene.h"
#include "loaders/obj.h"
#include "core/thread_pool.h"

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

static inline struct bbox get_tri_bbox(void* primitive_data, size_t index) {
    const struct tri* tri = &((struct tri*)primitive_data)[index];
    return extend_bbox(extend_bbox(point_bbox(tri->p0), get_tri_p1(tri)), get_tri_p2(tri));
}

static inline struct vec3 get_tri_center(void* primitive_data, size_t index) {
    const struct tri* tri = &((struct tri*)primitive_data)[index];
    return scale_vec3(add_vec3(tri->p0, add_vec3(get_tri_p1(tri), get_tri_p2(tri))), (real_t)1 / (real_t)3);
}

static inline struct bvh build_tri_bvh(struct thread_pool* thread_pool, struct tri* tris, size_t tri_count) {
    return build_bvh(thread_pool, tris, get_tri_bbox, get_tri_center, tri_count);
}

struct scene* load_scene(const char* file_name) {
    struct obj* obj = load_obj(file_name);
    if (!obj)
        return NULL;
    struct scene* scene = xmalloc(sizeof(struct scene));
    scene->tri_count = count_triangles(obj);
    scene->tris = xmalloc(sizeof(struct tri) * scene->tri_count);
    fill_tris(scene->tris, obj);
    free_obj(obj);

    struct timespec t_start;
    timespec_get(&t_start, TIME_UTC);
    struct thread_pool* thread_pool = new_thread_pool(detect_system_thread_count());
    scene->bvh = build_tri_bvh(thread_pool, scene->tris, scene->tri_count);
    free_thread_pool(thread_pool);
    struct timespec t_end;
    timespec_get(&t_end, TIME_UTC);

    printf("Building BVH took %g ms\n", elapsed_seconds(&t_start, &t_end) * 1.e3);
    return scene;
}

void free_scene(struct scene* scene) {
    free_bvh(&scene->bvh);
    free(scene->tris);
    free(scene);
}
