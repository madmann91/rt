#include "scene/scene.h"
#include "scene/mesh.h"
#include "core/tri.h"
#include "core/quad.h"
#include "core/hash_table.h"
#include "core/hash.h"
#include "io/obj_model.h"

static void count_primitives(const struct obj_model* model, size_t* tri_count, size_t* quad_count) {
    for (size_t i = 0, n = model->face_count; i < n; ++i) {
        const struct obj_face* face = &model->faces[i];
        *tri_count  += face->index_count < 3 ? 1 : 1 + face->index_count - 3;
        *quad_count += face->index_count < 4 ? 1 : 1 + round_up(face->index_count - 4, 2);
    }
}

GEN_DEFAULT_HASH(obj_index, struct obj_index)
GEN_DEFAULT_COMPARE(obj_index, struct obj_index)

static size_t compute_unique_vertices(
    const struct obj_model* model,
    struct hash_table* index_table,
    bool* has_normals, bool* has_tex_coords)
{
    // This maps unique triplets (v, t, n) to a unique vertex,
    // in order to match the way mesh indices work.
    for (size_t i = 0, n = model->index_count; i < n; ++i) {
        const struct obj_index* index = &model->indices[i];
        *has_normals |= index->n != 0;
        *has_tex_coords |= index->t != 0;
        size_t index_count = index_table->size;
        insert_in_hash_table(
            index_table,
            index, sizeof(struct obj_index),
            &index_count, sizeof(size_t),
            hash_obj_index(index),
            compare_obj_index);
    }
    return index_table->size;
}

static size_t translate_obj_index(const struct hash_table* index_table, const struct obj_index* index) {
    size_t value_index = find_in_hash_table(
        index_table,
        index, sizeof(struct obj_index),
        hash_obj_index(index),
        compare_obj_index);
    assert(value_index != SIZE_MAX);
    return ((const size_t*)index_table->values)[value_index];
}

static const enum attr_type obj_attr_types[] = {
#define f(name, type, ...) \
    ATTR_##type,
STANDARD_ATTR_LIST(f)
#undef f
    ATTR_VEC2
};
static const enum attr_binding obj_attr_bindings[] = {
#define f(name, type, binding) \
    PER_##binding,
STANDARD_ATTR_LIST(f)
#undef f
    PER_VERTEX
};

static struct mesh* build_mesh_from_obj_model(const struct obj_model* model) {
    size_t tri_count = 0, quad_count = 0;
    count_primitives(model, &tri_count, &quad_count);

    struct hash_table* index_table = new_hash_table(sizeof(struct obj_index), sizeof(size_t));
    bool has_normals = false, has_tex_coords = false;
    size_t vertex_count = compute_unique_vertices(model, index_table, &has_normals, &has_tex_coords);

    // TODO: Debug this
    //bool should_use_quads = sizeof(struct quad) * quad_count <= sizeof(struct tri) * tri_count;
    bool should_use_quads = false;
    size_t attr_count = has_tex_coords ? ARRAY_SIZE(obj_attr_bindings) : ARRAY_SIZE(obj_attr_bindings) - 1;
    struct mesh* mesh = new_mesh(
        should_use_quads ? QUAD_MESH : TRI_MESH,
        should_use_quads ? quad_count : tri_count,
        vertex_count, obj_attr_types, obj_attr_bindings, attr_count);

    // Copy vertices from the model into the mesh
    struct vec3* vertices   = mesh->attrs[ATTR_POSITION].data;
    struct vec3* normals    = mesh->attrs[ATTR_SHADING_NORMAL].data;
    struct vec2* tex_coords = has_tex_coords ? mesh->attrs[mesh->attr_count - 1].data : NULL;
    for (size_t i = 0, n = index_table->cap; i < n; ++i) {
        if (!is_bucket_occupied(index_table, i))
            continue;
        size_t j = ((size_t*)index_table->values)[i];
        struct obj_index* obj_index = &((struct obj_index*)index_table->keys)[i];
        vertices[j] = model->vertices[obj_index->v];
        if (has_normals)    normals[j]    = model->normals[obj_index->n];
        if (has_tex_coords) tex_coords[j] = model->tex_coords[obj_index->t];
    }

    // Compute face indices
    for (size_t i = 0, k = 0, n = model->face_count; i < n; ++i) {
        const struct obj_face* face = &model->faces[i];
        assert(face->index_count >= 3);
        size_t i0 = translate_obj_index(index_table, &model->indices[face->first_index + 0]);
        size_t i1 = translate_obj_index(index_table, &model->indices[face->first_index + 1]);
        if (should_use_quads) {
            for (size_t j = 2, m = face->index_count; j < m; j += 2) {
                assert(k < mesh->primitive_count);
                size_t i2 = translate_obj_index(index_table, &model->indices[face->first_index + j]);
                size_t i3 = i2;
                if (j + 1 < face->index_count) 
                    i3 = translate_obj_index(index_table, &model->indices[face->first_index + j + 1]);
                mesh->indices[k * 4 + 0] = i0;
                mesh->indices[k * 4 + 1] = i1;
                mesh->indices[k * 4 + 2] = i2;
                mesh->indices[k * 4 + 3] = i3;
                i1 = i3;
                k++;
            }
        } else {
            for (size_t j = 2, m = face->index_count; j < m; ++j) {
                assert(k < mesh->primitive_count);
                size_t i2 = translate_obj_index(index_table, &model->indices[face->first_index + j]);
                mesh->indices[k * 3 + 0] = i0;
                mesh->indices[k * 3 + 1] = i1;
                mesh->indices[k * 3 + 2] = i2;
                i1 = i2;
                k++;
            }
        }
    }
    free_hash_table(index_table);
    recompute_geometry_normals(mesh);
    if (!has_normals)
        recompute_shading_normals(mesh);
    return mesh;
}

struct mesh* import_obj_model(struct scene* scene, const char* file_name) {
    // TODO: Convert materials
    IGNORE(scene);
    struct obj_model* model = load_obj_model(file_name);
    if (!model)
        return NULL;
    struct mesh* mesh = build_mesh_from_obj_model(model);
    free_obj_model(model);
    return mesh;
}
