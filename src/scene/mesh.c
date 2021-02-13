#include <stdlib.h>

#include "scene/mesh.h"
#include "accel/bvh.h"
#include "accel/accel.h"
#include "core/thread_pool.h"
#include "core/ray.h"
#include "core/bbox.h"
#include "core/tri.h"
#include "core/quad.h"

struct mesh_accel {
    struct accel accel;
    struct bvh* bvh;
    void* primitives;
};

struct mesh* new_mesh(
    enum mesh_type mesh_type,
    size_t primitive_count,
    size_t vertex_count,
    const enum attr_type* attr_types,
    const enum attr_binding* attr_bindings,
    size_t attr_count)
{
#ifndef NDEBUG
    {
        size_t i = 0;
        // Check that standard attributes are present
#define f(name, type, binding) \
        assert(i < attr_count && attr_types[i] == ATTR_##type && attr_bindings[i] == PER_##binding), i++;
        STANDARD_ATTR_LIST(f)
#undef f
    }
#endif
    struct mesh* mesh = xmalloc(sizeof(struct mesh));
    mesh->type = mesh_type;
    mesh->indices = xmalloc(sizeof(size_t) * primitive_count * (mesh_type == TRI_MESH ? 3 : 4));
    mesh->attrs = xmalloc(sizeof(struct attr_buf) * attr_count);
    for (size_t i = 0; i < attr_count; ++i) {
        size_t attr_elem_count = attr_bindings[i] == PER_FACE ? primitive_count : vertex_count;
        mesh->attrs[i].data = xmalloc(get_attr_size(attr_types[i]) * attr_elem_count);
        mesh->attrs[i].type = attr_types[i];
        mesh->attrs[i].binding = attr_bindings[i];
    }
    mesh->primitive_count = primitive_count;
    mesh->attr_count = attr_count;
    mesh->vertex_count = vertex_count;
    return mesh;
}

void free_mesh(struct mesh* mesh) {
    for (size_t i = 0, n = mesh->attr_count; i < n; ++i)
        free(mesh->attrs[i].data);
    free(mesh->indices);
    free(mesh->attrs);
    free(mesh);
}

static inline union attr interpolate_uint(
    const struct mesh* mesh,
    const struct attr_buf* attr_buf,
    size_t primitive_index,
    const struct vec2* uv)
{
    // Only floating point data can be interpolated.
    IGNORE(mesh);
    IGNORE(attr_buf);
    IGNORE(primitive_index);
    IGNORE(uv);
    assert(false);
    return (union attr) { .uint = 0 };
}

#define GEN_INTERPOLATE(T, name) \
    static inline union attr interpolate_##name( \
        const struct mesh* mesh, \
        const struct attr_buf* attr_buf, \
        size_t primitive_index, \
        const struct vec2* uv) \
    { \
        const T* data = attr_buf->data; \
        if (mesh->type == TRI_MESH) { \
            T x = data[mesh->indices[primitive_index * 3 + 0]]; \
            T y = data[mesh->indices[primitive_index * 3 + 1]]; \
            T z = data[mesh->indices[primitive_index * 3 + 2]]; \
            return (union attr) { .name = lerp3_##name(x, y, z, uv->_[0], uv->_[1]) }; \
        } else { \
            T x = data[mesh->indices[primitive_index * 4 + 0]]; \
            T y = data[mesh->indices[primitive_index * 4 + 1]]; \
            T z = data[mesh->indices[primitive_index * 4 + 2]]; \
            T w = data[mesh->indices[primitive_index * 4 + 3]]; \
            return (union attr) { .name = lerp4_##name(x, y, z, w, uv->_[0], uv->_[1]) }; \
        } \
    }

GEN_INTERPOLATE(real_t, real)
GEN_INTERPOLATE(struct vec2, vec2)
GEN_INTERPOLATE(struct vec3, vec3)
GEN_INTERPOLATE(struct vec4, vec4)

union attr get_mesh_attr(
    const struct mesh* mesh,
    unsigned attr_index,
    size_t primitive_index,
    const struct vec2* uv)
{
    assert(attr_index < mesh->attr_count);
    assert(primitive_index < mesh->primitive_count);
    const struct attr_buf* attr_buf = &mesh->attrs[attr_index];
    if (attr_buf->binding == PER_FACE) {
        // The attribute is per-face, no need to interpolate
        switch (attr_buf->type) {
#define f(name, tag, type) \
            case ATTR_##tag: \
                return (union attr) { .name = ((const type*)attr_buf->data)[primitive_index] };
            ATTR_LIST(f)
#undef f
            default:
                assert(false);
                return (union attr) { .uint = 0 };
        }
    } else {
        // The attribute is per-vertex. It requires interpolation.
        assert(attr_buf->binding == PER_VERTEX);
        switch (attr_buf->type) {
#define f(name, tag, type) \
            case ATTR_##tag: \
                return interpolate_##name(mesh, attr_buf, primitive_index, uv);
            ATTR_LIST(f)
#undef f
            default:
                assert(false);
                return (union attr) { .uint = 0 };
        }
    }
}

void recompute_shading_normals(struct mesh* mesh) {
    const struct vec3* geometry_normals = mesh->attrs[ATTR_GEOMETRY_NORMAL].data;
    struct vec3* normals = mesh->attrs[ATTR_SHADING_NORMAL].data;
    size_t index_stride = mesh->type == TRI_MESH ? 3 : 4;
    for (size_t i = 0; i < mesh->vertex_count; ++i)
        normals[i] = const_vec3(0);
    for (size_t i = 0; i < mesh->primitive_count; i++) {
        const struct vec3 geometry_normal = geometry_normals[i];
        for (size_t j = 0; j < index_stride; ++j) {
            size_t k = mesh->indices[i * index_stride + j];
            normals[k] = add_vec3(normals[k], geometry_normal);
        }
    }
    for (size_t i = 0; i < mesh->vertex_count; ++i)
        normals[i] = normalize_vec3(normals[i]);
}

void recompute_geometry_normals(struct mesh* mesh) {
    struct vec3* geometry_normals = mesh->attrs[ATTR_GEOMETRY_NORMAL].data;
    const struct vec3* vertices = mesh->attrs[ATTR_POSITION].data;
    size_t index_stride = mesh->type == TRI_MESH ? 3 : 4;
    for (size_t i = 0; i < mesh->primitive_count; ++i) {
        const struct vec3 v0 = vertices[mesh->indices[i * index_stride + 0]];
        const struct vec3 v1 = vertices[mesh->indices[i * index_stride + 1]];
        const struct vec3 v2 = vertices[mesh->indices[i * index_stride + 2]];
        geometry_normals[i] = normalize_vec3(cross_vec3(sub_vec3(v1, v0), sub_vec3(v2, v0)));
    }
}

static struct bbox get_tri_bbox(void* primitive_data, size_t index) {
    const struct tri* tri = &((const struct tri*)primitive_data)[index];
    return
        union_bbox(point_bbox(tri->p0),
        union_bbox(point_bbox(get_tri_p1(tri)),
            point_bbox(get_tri_p2(tri))));
}

static struct bbox get_quad_bbox(void* primitive_data, size_t index) {
    const struct quad* quad = &((const struct quad*)primitive_data)[index];
    return
        union_bbox(point_bbox(quad->p0),
        union_bbox(point_bbox(get_quad_p1(quad)),
        union_bbox(point_bbox(get_quad_p2(quad)),
            point_bbox(get_quad_p3(quad)))));
}

static struct vec3 get_tri_center(void* primitive_data, size_t index) {
    const struct tri* tri = &((const struct tri*)primitive_data)[index];
    return scale_vec3(add_vec3(tri->p0, add_vec3(get_tri_p1(tri), get_tri_p2(tri))), 1.0 / 3.0);
}

static struct vec3 get_quad_center(void* primitive_data, size_t index) {
    const struct quad* quad = &((const struct quad*)primitive_data)[index];
    return scale_vec3(
        add_vec3(
            add_vec3(quad->p0, get_quad_p1(quad)),
            add_vec3(get_quad_p2(quad), get_quad_p3(quad))),
        1.0 / 4.0);
}

#define GEN_INTERSECT_RAY_MESH_ACCEL(T, intersect_ray_primitive) \
    static inline bool intersect_ray_##T##_mesh_accel_leaf( \
        struct ray* ray, struct hit* hit, \
        const struct bvh_node* leaf, \
        const struct T* primitives, bool any) \
    { \
        bool found = false; \
        for (size_t i = leaf->first_child_or_primitive, n = i + leaf->primitive_count; i < n; ++i) { \
            if (intersect_ray_primitive(ray, hit, &primitives[i])) { \
                hit->primitive_index = i; \
                found = true; \
                if (any) \
                    return true; \
            } \
        } \
        return found; \
    } \
    static bool intersect_ray_##T##_mesh_accel_leaf_closest( \
        struct ray* ray, struct hit* hit, \
        const struct bvh_node* leaf, \
        void* intersection_data) \
    { \
        return intersect_ray_##T##_mesh_accel_leaf(ray, hit, leaf, intersection_data, false); \
    } \
    static bool intersect_ray_##T##_mesh_accel_leaf_any( \
        struct ray* ray, struct hit* hit, \
        const struct bvh_node* leaf, \
        void* intersection_data) \
    { \
        return intersect_ray_##T##_mesh_accel_leaf(ray, hit, leaf, intersection_data, true); \
    } \
    static bool intersect_ray_##T##_mesh_accel(struct ray* ray, struct hit* hit, const struct accel* accel, bool any) { \
        struct mesh_accel* mesh_accel = (void*)accel; \
        if (intersect_ray_bvh( \
            ray, hit, \
            mesh_accel->bvh, \
            any \
                ? intersect_ray_##T##_mesh_accel_leaf_any \
                : intersect_ray_##T##_mesh_accel_leaf_closest, \
            mesh_accel->primitives, any)) { \
            hit->primitive_index = mesh_accel->bvh->primitive_indices[hit->primitive_index]; \
            return true; \
        } \
        return false; \
    }

GEN_INTERSECT_RAY_MESH_ACCEL(tri, intersect_ray_tri)
GEN_INTERSECT_RAY_MESH_ACCEL(quad, intersect_ray_quad)

static void free_mesh_accel(struct accel* accel) {
    struct mesh_accel* mesh_accel = (void*)accel;
    free(mesh_accel->primitives);
    free_bvh(mesh_accel->bvh);
    free(mesh_accel);
}

struct permute_task {
    struct parallel_task_1d task;
    const void* src_primitives;
    void* dst_primitives;
    const size_t* primitive_indices;
};

#define GEN_PERMUTE_TASK(T) \
    static void run_permute_##T##s_task(struct parallel_task_1d* task, size_t thread_id) { \
        IGNORE(thread_id); \
        struct permute_task* permute_task = (void*)task; \
        for (size_t i = task->range.begin, n = task->range.end; i < n; ++i) \
            ((struct T*)permute_task->dst_primitives)[i] = \
                ((struct T*)permute_task->src_primitives)[permute_task->primitive_indices[i]]; \
    } \

GEN_PERMUTE_TASK(tri)
GEN_PERMUTE_TASK(quad)

static inline void permute_primitives(
    struct thread_pool* thread_pool,
    void (*run_permute_task)(struct parallel_task_1d*, size_t),
    const size_t* primitive_indices,
    const void* src_primitives,
    void* dst_primitives,
    size_t primitive_count)
{
    parallel_for_1d(
        thread_pool,
        run_permute_task,
        (struct parallel_task_1d*)&(struct permute_task) {
            .src_primitives = src_primitives,
            .dst_primitives = dst_primitives,
            .primitive_indices = primitive_indices
        }, \
        sizeof(struct permute_task),
        &(struct range) { 0, primitive_count });
}

struct init_primitives_task {
    struct parallel_task_1d task;
    void* primitives;
    const struct mesh* mesh;
};

static void run_init_tris_task(struct parallel_task_1d* task, size_t thread_id) {
    IGNORE(thread_id);
    struct init_primitives_task* init_primitives_task = (void*)task;
    struct tri* tris = init_primitives_task->primitives;
    const struct mesh* mesh = init_primitives_task->mesh;
    const struct vec3* vertices = mesh->attrs[ATTR_POSITION].data;
    for (size_t i = task->range.begin, n = task->range.end; i < n; ++i) {
        const struct vec3* v0 = &vertices[mesh->indices[i * 3 + 0]];
        const struct vec3* v1 = &vertices[mesh->indices[i * 3 + 1]];
        const struct vec3* v2 = &vertices[mesh->indices[i * 3 + 2]];
        tris[i] = make_tri(v0, v1, v2);
    }
}

static void run_init_quads_task(struct parallel_task_1d* task, size_t thread_id) {
    IGNORE(thread_id);
    struct init_primitives_task* init_primitives_task = (void*)task;
    struct quad* quads = init_primitives_task->primitives;
    const struct mesh* mesh = init_primitives_task->mesh;
    const struct vec3* vertices = mesh->attrs[ATTR_POSITION].data;
    for (size_t i = task->range.begin, n = task->range.end; i < n; ++i) {
        const struct vec3* v0 = &vertices[mesh->indices[i * 4 + 0]];
        const struct vec3* v1 = &vertices[mesh->indices[i * 4 + 1]];
        const struct vec3* v2 = &vertices[mesh->indices[i * 4 + 2]];
        const struct vec3* v3 = &vertices[mesh->indices[i * 4 + 3]];
        quads[i] = make_quad(v0, v1, v2, v3);
    }
}

static inline void init_primitives(
    struct thread_pool* thread_pool,
    void (*run_init_primitives_task)(struct parallel_task_1d*, size_t),
    const struct mesh* mesh,
    size_t begin, size_t end,
    void* primitives)
{
    parallel_for_1d(
        thread_pool,
        run_init_primitives_task,
        (struct parallel_task_1d*)&(struct init_primitives_task) {
            .primitives = primitives,
            .mesh = mesh
        },
        sizeof(struct init_primitives_task),
        &(struct range) { begin, end });
}

#define GEN_BUILD_MESH_ACCEL(T, mesh_type, traversal_cost) \
    static struct accel* build_##T##_mesh_accel( \
        struct thread_pool* thread_pool, \
        const struct mesh* mesh, \
        size_t begin, size_t end) \
    { \
        assert(mesh->type == mesh_type); \
        size_t primitive_count = end - begin; \
        struct T* primitives = xmalloc(sizeof(struct T) * primitive_count); \
        init_primitives(thread_pool, run_init_##T##s_task, mesh, begin, end, primitives - begin); \
        struct bvh* bvh = build_bvh( \
            thread_pool, primitives, \
            get_##T##_bbox, \
            get_##T##_center, \
            primitive_count, \
            traversal_cost); \
        struct T* permuted_primitives = xmalloc(sizeof(struct T) * primitive_count); \
        permute_primitives( \
            thread_pool, \
            run_permute_##T##s_task, \
            bvh->primitive_indices, \
            primitives, permuted_primitives, \
            primitive_count); \
        free(primitives); \
        struct mesh_accel* mesh_accel = xmalloc(sizeof(struct mesh_accel)); \
        mesh_accel->accel.intersect_ray = intersect_ray_##T##_mesh_accel; \
        mesh_accel->accel.free = free_mesh_accel; \
        mesh_accel->bvh = bvh; \
        mesh_accel->primitives = permuted_primitives; \
        return &mesh_accel->accel; \
    }

GEN_BUILD_MESH_ACCEL(tri,  TRI_MESH,  1.5)
GEN_BUILD_MESH_ACCEL(quad, QUAD_MESH, 1.2)

struct accel* build_mesh_accel(struct thread_pool* thread_pool, const struct mesh* mesh, size_t begin, size_t end) {
    return mesh->type == TRI_MESH
        ? build_tri_mesh_accel(thread_pool, mesh, begin, end)
        : build_quad_mesh_accel(thread_pool, mesh, begin, end);
}
