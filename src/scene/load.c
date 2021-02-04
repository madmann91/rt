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
        struct vec3 v0 = obj->vertices[obj->indices[face->first_index + 0].v];
        struct vec3 v1 = obj->vertices[obj->indices[face->first_index + 1].v];
        for (size_t j = 2; j < face->index_count; ++j) {
            struct vec3 v2 = obj->vertices[obj->indices[face->first_index + j].v];
            tris[tri_count++] = make_tri(v0, v1, v2);
            v1 = v2;
        }
    }
}

static inline struct bbox get_tri_bbox(void* primitive_data, size_t index) {
    const struct tri* tri = &((struct tri*)primitive_data)[index];
    return extend_bbox(extend_bbox(point_bbox(tri->p0), tri_p1(tri)), tri_p2(tri));
}

static inline struct vec3 get_tri_center(void* primitive_data, size_t index) {
    const struct tri* tri = &((struct tri*)primitive_data)[index];
    return scale_vec3(add_vec3(tri->p0, add_vec3(tri_p1(tri), tri_p2(tri))), (real_t)1 / (real_t)3);
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

    struct thread_pool* thread_pool = new_thread_pool(detect_system_thread_count());
    scene->bvh = build_tri_bvh(thread_pool, scene->tris, scene->tri_count);
    free_thread_pool(thread_pool);
    return scene;
}
