#ifndef IO_OBJ_H
#define IO_OBJ_H

#include <stdbool.h>

#include "core/vec3.h"
#include "core/vec2.h"
#include "core/rgb.h"

struct obj_index {
    long long v, n, t;
};

struct obj_face {
    size_t first_index;
    size_t index_count;
};

struct obj_group {
    size_t first_face;
    size_t face_count;
    size_t material_index;
};

struct obj_model {
    struct obj_group* groups;
    struct obj_face* faces;
    struct obj_index* indices;
    struct vec3* vertices;
    struct vec3* normals;
    struct vec2* tex_coords;

    size_t group_count;
    size_t face_count;
    size_t index_count;
    size_t vertex_count;
    size_t normal_count;
    size_t tex_coord_count;

    char** material_names;
    char** mtl_file_names;

    size_t material_count;
    size_t mtl_file_count;
};

struct mtl_material {
    char* name;
    struct rgb ka;
    struct rgb kd;
    struct rgb ks;
    struct rgb ke;
    real_t ns;
    real_t ni;
    struct rgb tf;
    real_t tr;
    real_t d;
    int illum;
    char* map_ka;
    char* map_kd;
    char* map_ks;
    char* map_ke;
    char* map_bump;
    char* map_d;
};

struct mtl_lib {
    struct mtl_material* materials;
    size_t material_count;
};

struct obj_model* load_obj_model(const char* file_name);
struct mtl_lib* load_mtl_lib(const char* file_name);
void free_obj_model(struct obj_model*);
void free_mtl_lib(struct mtl_lib*);

#endif
