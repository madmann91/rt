#include "scene/geometry.h"
#include "accel/accel.h"
#include "core/mem_pool.h"
#include "core/utils.h"
#include "core/hash.h"

struct geometry {
    struct scene_node node;

    void (*prepare)(geometry_t, struct thread_pool*);
    bool (*intersect_ray)(geometry_t, struct ray*, struct hit*, bool any);
    union attr (*get_attr)(geometry_t, unsigned, const struct ray*, const struct hit*);
    struct surface_sample (*sample_surface)(geometry_t, const struct vec2*);
    real_t (*get_surface_area)(geometry_t);
};

void prepare_geometry(geometry_t geometry, struct thread_pool* thread_pool) {
    geometry->prepare(geometry, thread_pool);
}

bool intersect_ray_geometry(struct ray* ray, struct hit* hit, geometry_t geometry, bool any) {
    return geometry->intersect_ray(geometry, ray, hit, any);
}

union attr get_geometry_attr(geometry_t geometry, unsigned attr_index, const struct ray* ray, const struct hit* hit) {
    return geometry->get_attr(geometry, attr_index, ray, hit);
}

struct surface_sample sample_geometry_surface(geometry_t geometry, const struct vec2* uv) {
    return geometry->sample_surface(geometry, uv);
}

real_t get_geometry_surface_area(geometry_t geometry) {
    return geometry->get_surface_area(geometry);
}

struct submesh_geometry {
    struct geometry geometry;
    const struct mesh* mesh;
    size_t begin, end;
    struct accel* accel;
};

geometry_t new_mesh_geometry(struct scene* scene, const struct mesh* mesh) {
    return new_submesh_geometry(scene, mesh, 0, mesh->primitive_count);
}

static void prepare_submesh_geometry(geometry_t geometry, struct thread_pool* thread_pool) {
    struct submesh_geometry* submesh_geometry = (void*)geometry;
    submesh_geometry->accel = build_mesh_accel(
        thread_pool,
        submesh_geometry->mesh,
        submesh_geometry->begin,
        submesh_geometry->end);
}

static bool intersect_submesh_geometry_ray(geometry_t geometry, struct ray* ray, struct hit* hit, bool any) {
    struct submesh_geometry* submesh_geometry = (void*)geometry;
    assert(submesh_geometry->accel);
    return intersect_ray_accel(ray, hit, submesh_geometry->accel, any);
}

static union attr get_submesh_geometry_attr(
    geometry_t geometry, unsigned attr_index,
    const struct ray* ray, const struct hit* hit)
{
    IGNORE(ray);
    struct submesh_geometry* submesh_geometry = (void*)geometry;
    return get_mesh_attr(
        submesh_geometry->mesh,
        attr_index,
        submesh_geometry->begin + hit->primitive_index,
        &hit->uv);
}

static struct surface_sample sample_submesh_geometry_surface(geometry_t geometry, const struct vec2* uv) {
    // TODO
    IGNORE(geometry);
    IGNORE(uv);
    assert(false);
    return (struct surface_sample) {
        .pdf = 0
    };
}

static real_t get_submesh_geometry_surface_area(geometry_t geometry) {
    // TODO
    IGNORE(geometry);
    assert(false);
    return 0;
}

static uint32_t hash_submesh_geometry(const struct scene_node* node) {
    assert(node->type == SUBMESH_GEOMETRY);
    struct submesh_geometry* submesh_geometry = (void*)node;
    return hash_uint(hash_uint(hash_ptr(hash_init(),
        submesh_geometry->mesh),
        submesh_geometry->begin),
        submesh_geometry->end);
}

static bool compare_submesh_geometry(const struct scene_node* left, const struct scene_node* right) {
    assert(left->type == SUBMESH_GEOMETRY && left->type == right->type);
    struct submesh_geometry* left_submesh_geometry  = (void*)left;
    struct submesh_geometry* right_submesh_geometry = (void*)right;
    return
        left_submesh_geometry->mesh  == right_submesh_geometry->mesh &&
        left_submesh_geometry->begin == right_submesh_geometry->begin &&
        left_submesh_geometry->end   == right_submesh_geometry->end;
}

static void cleanup_submesh_geometry(struct scene_node* node) {
    struct submesh_geometry* submesh_geometry  = (void*)node;
    free_accel(submesh_geometry->accel);
}

geometry_t new_submesh_geometry(struct scene* scene, const struct mesh* mesh, size_t begin, size_t end) {
    struct submesh_geometry submesh_geometry = {
        .geometry = {
            .node = {
                .type    = SUBMESH_GEOMETRY,
                .hash    = hash_submesh_geometry,
                .compare = compare_submesh_geometry,
                .cleanup = cleanup_submesh_geometry
            },
            .prepare          = prepare_submesh_geometry,
            .intersect_ray    = intersect_submesh_geometry_ray,
            .get_attr         = get_submesh_geometry_attr,
            .sample_surface   = sample_submesh_geometry_surface,
            .get_surface_area = get_submesh_geometry_surface_area
        },
        .mesh = mesh,
        .begin = begin,
        .end = end
    };
    return (geometry_t)insert_scene_node(scene,
        &submesh_geometry.geometry.node,
        sizeof(submesh_geometry));
}
