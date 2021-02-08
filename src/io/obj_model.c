#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>

#include "io/obj_model.h"
#include "core/utils.h"

#define LINE_BUF_SIZE 1024
#define ARRAY(T, name) \
    struct { T* data; size_t size; size_t cap; } name = { NULL, 0, 0 };
#define PUSH(name, ...) \
    do { \
        if (name.size >= name.cap) { \
            name.cap = (name.cap + 1) * 2; \
            name.data = xrealloc(name.data, name.cap * sizeof(__VA_ARGS__)); \
        } \
        name.data[name.size++] = __VA_ARGS__; \
    } while (false)

static inline void remove_spaces_after(char* ptr) {
    int i = 0;
    while (ptr[i]) i++;
    i--;
    while (i > 0 && isspace(ptr[i])) {
        ptr[i] = '\0';
        i--;
    }
}

static inline char* skip_text(char* ptr) {
    while (*ptr && !isspace(*ptr)) { ptr++; }
    return ptr;
}

static inline char* skip_spaces(char* ptr) {
    while (isspace(*ptr)) { ptr++; }
    return ptr;
}

static inline bool read_index(char** ptr, struct obj_index* idx) {
    char* base = *ptr;

    // Detect end of line (negative indices are supported)
    base = skip_spaces(base);
    if (!isdigit(*base) && *base != '-')
        return false;

    idx->t = idx->n = 0;

    idx->v = strtoll(base, &base, 10);
    base = skip_spaces(base);

    if (*base == '/') {
        base++;

        // Handle the case when there is no texture coordinate
        if (*base != '/')
            idx->t = strtoll(base, &base, 10);

        base = skip_spaces(base);

        if (*base == '/') {
            base++;
            idx->n = strtoll(base, &base, 10);
        }
    }

    *ptr = base;
    return idx->v != 0;
}

static inline size_t find_name(char** names, size_t name_count, const char* name) {
    for (size_t i = 0; i < name_count; ++i) {
        if (!strcmp(names[i], name))
            return i;
    }
    return SIZE_MAX;
}

static bool parse_obj(FILE* fp, const char* file_name, struct obj_model* model) {
    ARRAY(struct obj_face, faces)
    ARRAY(struct obj_group, groups)
    ARRAY(struct obj_index, indices)
    ARRAY(struct vec3, vertices)
    ARRAY(struct vec3, normals)
    ARRAY(struct vec2, tex_coords)
    ARRAY(char*, material_names)
    ARRAY(char*, mtl_file_names)

    // Create a dummy material with a dummy group in case the model has no materials
    PUSH(groups, (struct obj_group) { .first_face = 0, .material_index = 0 });
    PUSH(material_names, copy_str("#dummy"));

    // Reserve dummy vertices since indices start at 1
    PUSH(vertices,   const_vec3(0));
    PUSH(normals,    const_vec3(0));
    PUSH(tex_coords, const_vec2(0));

    char line_buf[LINE_BUF_SIZE];
    size_t line_count = 0;
    bool ok = true;

    while (fgets(line_buf, LINE_BUF_SIZE, fp)) {
        line_count++;

        char* ptr = skip_spaces(line_buf);
        // Skip comments and empty lines
        if (*ptr == '\0' || *ptr == '#')
            continue;
        remove_spaces_after(ptr);

        // Test each command in turn, the most frequent first
        if (*ptr == 'v') {
            switch (ptr[1]) {
                case ' ':
                case '\t': {
                    real_t x = strtoreal(ptr + 1, &ptr);
                    real_t y = strtoreal(ptr, &ptr);
                    real_t z = strtoreal(ptr, &ptr);
                    PUSH(vertices, (struct vec3) { { x, y, z } });
                    break;
                }
                case 'n': {
                    real_t x = strtoreal(ptr + 2, &ptr);
                    real_t y = strtoreal(ptr, &ptr);
                    real_t z = strtoreal(ptr, &ptr);
                    PUSH(normals, (struct vec3) { { x, y, z } });
                    break;
                }
                case 't': {
                    real_t x = strtoreal(ptr + 2, &ptr);
                    real_t y = strtoreal(ptr, &ptr);
                    PUSH(tex_coords, (struct vec2) { { x, y } });
                    break;
                }
                default:
                    fprintf(stderr, "invalid vertex in %s, line %zu\n", file_name, line_count);
                    ok = false;
                    break;
            }
        } else if (*ptr == 'f' && isspace(ptr[1])) {
            struct obj_face f = { .first_index = indices.size };
            ptr += 2;
            while (true) {
                struct obj_index index;
                if (read_index(&ptr, &index))
                    PUSH(indices, index);
                else
                    break;
            }
            f.index_count = indices.size - f.first_index;

            // Convert relative indices to absolute
            bool valid = f.index_count >= 3;
            for (size_t i = f.first_index, n = f.first_index + f.index_count; i < n && valid; i++) {
                struct obj_index* index = &indices.data[i];
                index->v = index->v < 0 ? (long long)vertices.size   + index->v : index->v;
                index->t = index->t < 0 ? (long long)tex_coords.size + index->t : index->t;
                index->n = index->n < 0 ? (long long)normals.size    + index->n : index->n;
                valid &= index->v >= 1;
                valid &= index->v <= (long long)vertices.size;
                valid &= index->t <= (long long)tex_coords.size;
                valid &= index->n <= (long long)normals.size;
            }

            if (valid)
                PUSH(faces, f);
            else
                fprintf(stderr, "invalid face in %s, line %zu\n", file_name, line_count);
        } else if (!strncmp(ptr, "usemtl", 6) && isspace(ptr[6])) {
            ptr = skip_spaces(ptr + 6);
            char* base = ptr;
            ptr = skip_text(ptr);

            char* material_name = copy_str_n(base, ptr - base);
            size_t material_index = find_name(material_names.data, material_names.size, material_name);
            if (material_index == SIZE_MAX) {
                material_index = material_names.size;
                PUSH(material_names, material_name);
            } else
                free(material_name);
            if (material_index != groups.data[groups.size - 1].material_index) {
                PUSH(groups, (struct obj_group) {
                    .material_index = material_index,
                    .first_face     = faces.size
                });
            }
        } else if (!strncmp(ptr, "mtllib", 6) && isspace(ptr[6])) {
            ptr = skip_spaces(ptr + 6);
            char* base = ptr;
            ptr = skip_text(ptr);

            char* mtl_file_name = copy_str_n(base, ptr - base);
            if (find_name(mtl_file_names.data, mtl_file_names.size, mtl_file_name) == SIZE_MAX)
                PUSH(mtl_file_names, mtl_file_name);
            else
                free(mtl_file_name);
        } else if ((*ptr == 'g' || *ptr == 'o' || *ptr == 's') && isspace(ptr[1])) {
            // Ignore the 'g', 'o', and 's' OBJ commands
        } else {
            fprintf(stderr, "invalid OBJ command '%s' in %s, line %zu\n", ptr, file_name, line_count);
            ok = false;
        }
    }

    model->groups     = groups.data;
    model->faces      = faces.data;
    model->indices    = indices.data;
    model->vertices   = vertices.data;
    model->normals    = normals.data;
    model->tex_coords = tex_coords.data;

    model->group_count     = groups.size;
    model->face_count      = faces.size;
    model->index_count     = indices.size;
    model->vertex_count    = vertices.size;
    model->normal_count    = normals.size;
    model->tex_coord_count = tex_coords.size;

    model->material_names = material_names.data;
    model->mtl_file_names = mtl_file_names.data;

    model->material_count = material_names.size;
    model->mtl_file_count = mtl_file_names.size;

    return ok;
}

static size_t find_material(const struct mtl_material* materials, size_t material_count, const char* name) {
    for (size_t i = 0; i < material_count; ++i) {
        if (!strcmp(materials[i].name, name))
            return i;
    }
    return SIZE_MAX;
}

static bool parse_mtl(FILE* fp, const char* file_name, struct mtl_lib* mtl_lib) {
    ARRAY(struct mtl_material, materials)

    char line_buf[LINE_BUF_SIZE];
    size_t line_count = 0;
    bool ok = true;

    while (fgets(line_buf, LINE_BUF_SIZE, fp)) {
        line_count++;

        char* ptr = skip_spaces(line_buf);
        // Skip comments and empty lines
        if (*ptr == '\0' || *ptr == '#')
            continue;
        remove_spaces_after(ptr);

        if (!strncmp(ptr, "newmtl", 6) && isspace(ptr[6])) {
            ptr = skip_spaces(ptr + 7);
            char* base = ptr;
            ptr = skip_text(ptr);

            char* material_name = copy_str_n(base, ptr - base);
            if (!find_material(materials.data, materials.size, material_name)) {
                fprintf(stderr, "material '%s' redefined in %s, line %zu\n", material_name, file_name, line_count);
                ok = false;
            }
            PUSH(materials, (struct mtl_material) { .name = material_name });
        } else if (materials.size == 0) {
            goto invalid_command;
        } else if (ptr[0] == 'K') {
            if (ptr[1] == 'a' && isspace(ptr[2])) {
                struct mtl_material* material = &materials.data[materials.size - 1];
                material->ka.r = strtoreal(ptr + 3, &ptr);
                material->ka.g = strtoreal(ptr, &ptr);
                material->ka.b = strtoreal(ptr, &ptr);
            } else if (ptr[1] == 'd' && isspace(ptr[2])) {
                struct mtl_material* material = &materials.data[materials.size - 1];
                material->kd.r = strtoreal(ptr + 3, &ptr);
                material->kd.g = strtoreal(ptr, &ptr);
                material->kd.b = strtoreal(ptr, &ptr);
            } else if (ptr[1] == 's' && isspace(ptr[2])) {
                struct mtl_material* material = &materials.data[materials.size - 1];
                material->ks.r = strtoreal(ptr + 3, &ptr);
                material->ks.g = strtoreal(ptr, &ptr);
                material->ks.b = strtoreal(ptr, &ptr);
            } else if (ptr[1] == 'e' && isspace(ptr[2])) {
                struct mtl_material* material = &materials.data[materials.size - 1];
                material->ke.r = strtoreal(ptr + 3, &ptr);
                material->ke.g = strtoreal(ptr, &ptr);
                material->ke.b = strtoreal(ptr, &ptr);
            } else
                goto invalid_command;
        } else if (ptr[0] == 'N') {
            if (ptr[1] == 's' && isspace(ptr[2])) {
                materials.data[materials.size - 1].ns = strtoreal(ptr + 3, &ptr);
            } else if (ptr[1] == 'i' && isspace(ptr[2])) {
                materials.data[materials.size - 1].ni = strtoreal(ptr + 3, &ptr);
            } else
                goto invalid_command;
        } else if (ptr[0] == 'T') {
            if (ptr[1] == 'f' && isspace(ptr[2])) {
                struct mtl_material* material = &materials.data[materials.size - 1];
                material->tf.r = strtoreal(ptr + 3, &ptr);
                material->tf.g = strtoreal(ptr, &ptr);
                material->tf.b = strtoreal(ptr, &ptr);
            } else if (ptr[1] == 'r' && isspace(ptr[2])) {
                materials.data[materials.size - 1].tr = strtoreal(ptr + 3, &ptr);
            } else
                goto invalid_command;
        } else if (ptr[0] == 'd' && isspace(ptr[1])) {
            materials.data[materials.size - 1].d = strtoreal(ptr + 2, &ptr);
        } else if (!strncmp(ptr, "illum", 5) && isspace(ptr[5])) {
            materials.data[materials.size - 1].illum = strtoreal(ptr + 6, &ptr);
        } else if (!strncmp(ptr, "map_Ka", 6) && isspace(ptr[6])) {
            materials.data[materials.size - 1].map_ka = copy_str(skip_spaces(ptr + 7));
        } else if (!strncmp(ptr, "map_Kd", 6) && isspace(ptr[6])) {
            materials.data[materials.size - 1].map_kd = copy_str(skip_spaces(ptr + 7));
        } else if (!strncmp(ptr, "map_Ks", 6) && isspace(ptr[6])) {
            materials.data[materials.size - 1].map_ks = copy_str(skip_spaces(ptr + 7));
        } else if (!strncmp(ptr, "map_Ke", 6) && isspace(ptr[6])) {
            materials.data[materials.size - 1].map_ke = copy_str(skip_spaces(ptr + 7));
        } else if (!strncmp(ptr, "map_bump", 8) && isspace(ptr[8])) {
            materials.data[materials.size - 1].map_bump = copy_str(skip_spaces(ptr + 9));
        } else if (!strncmp(ptr, "bump", 4) && isspace(ptr[4])) {
            materials.data[materials.size - 1].map_bump = copy_str(skip_spaces(ptr + 5));
        } else if (!strncmp(ptr, "map_d", 5) && isspace(ptr[5])) {
            materials.data[materials.size - 1].map_d = copy_str(skip_spaces(ptr + 6));
        } else
            goto invalid_command;
        continue;
invalid_command:
        fprintf(stderr, "invalid MTL command '%s' in %s, line %zu\n", ptr, file_name, line_count);
        ok = false;
    }

    mtl_lib->materials = materials.data;
    mtl_lib->material_count = materials.size;

    return ok;
}

struct obj_model* load_obj_model(const char* file_name) {
    FILE* fp = fopen(file_name, "r");
    if (!fp)
        return NULL;
    struct obj_model* model = xmalloc(sizeof(struct obj_model));
    if (!parse_obj(fp, file_name, model)) {
        free_obj_model(model);
        model = NULL;
    }
    fclose(fp);
    return model;
}

struct mtl_lib* load_mtl_lib(const char* file_name) {
    FILE* fp = fopen(file_name, "r");
    if (!fp)
        return NULL;
    struct mtl_lib* mtl_lib = xmalloc(sizeof(struct mtl_lib));
    if (!parse_mtl(fp, file_name, mtl_lib)) {
        free_mtl_lib(mtl_lib);
        mtl_lib = NULL;
    }
    fclose(fp);
    return mtl_lib;
}

void free_obj_model(struct obj_model* model) {
    free(model->groups);
    free(model->faces);
    free(model->indices);
    free(model->vertices);
    free(model->normals);
    free(model->tex_coords);
    for (size_t i = 0; i < model->material_count; ++i)
        free(model->material_names[i]);
    free(model->material_names);
    for (size_t i = 0; i < model->mtl_file_count; ++i)
        free(model->mtl_file_names[i]);
    free(model->mtl_file_names);
    free(model);
}

void free_mtl_lib(struct mtl_lib* mtl_lib) {
    for (size_t i = 0; i < mtl_lib->material_count; ++i) {
        free(mtl_lib->materials[i].name);
        free(mtl_lib->materials[i].map_ka);
        free(mtl_lib->materials[i].map_kd);
        free(mtl_lib->materials[i].map_ks);
        free(mtl_lib->materials[i].map_ke);
        free(mtl_lib->materials[i].map_bump);
        free(mtl_lib->materials[i].map_d);
    }
    free(mtl_lib->materials);
    free(mtl_lib);
}
