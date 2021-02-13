#ifndef SCENE_TRI_MESH_H
#define SCENE_TRI_MESH_H

#include <stddef.h>

#include "scene/attr.h"

struct accel;
struct thread_pool;

struct attr_buf {
    enum attr_binding {
        PER_FACE,
        PER_VERTEX
    } binding;
    enum attr_type type;
    void* data;
};

struct mesh {
    enum mesh_type {
        TRI_MESH,
        QUAD_MESH
    } type;
    size_t* indices;
    struct attr_buf* attrs;
    size_t attr_count;
    size_t vertex_count;
    size_t primitive_count;
};

struct mesh* new_mesh(
    enum mesh_type mesh_type,
    size_t primitive_count,
    size_t vertex_count,
    const enum attr_type* attr_types,
    const enum attr_binding* attr_bindings,
    size_t attr_count);

void free_mesh(struct mesh* mesh);

// Obtains the mesh attribute for a given hit on this mesh.
// Per-vertex attributes are automaticall interpolated by this function.
union attr get_mesh_attr(
    const struct mesh* mesh,
    unsigned attr_index,
    size_t primitive_index,
    const struct vec2* uv);

// Recomputes per-vertex shading normals based on the geometry normals.
void recompute_shading_normals(struct mesh* mesh);

// Recomputes geometry normals based on the vertex data
// (the winding order of vertices determines the normal direction).
void recompute_geometry_normals(struct mesh* mesh);

// Returns an acceleration data structure suitable to intersect
// the given mesh for the given primitive range.
// The returned object must be freed by calling `free_accel()`.
struct accel* build_mesh_accel(struct thread_pool*, const struct mesh*, size_t, size_t);

#endif
