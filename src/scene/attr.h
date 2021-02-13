#ifndef SCENE_ATTR_H
#define SCENE_ATTR_H

#include <assert.h>
#include <stdbool.h>

#include "core/vec2.h"
#include "core/vec3.h"
#include "core/vec4.h"

// List of attributes available for a mesh
#define ATTR_LIST(f) \
    f(real, REAL, real_t) \
    f(uint, UINT, uint32_t) \
    f(vec2, VEC2, struct vec2) \
    f(vec3, VEC3, struct vec3) \
    f(vec4, VEC4, struct vec4)

// The type of an attribute
union attr {
#define f(name, tag, type) type name;
    ATTR_LIST(f)
#undef f
};

enum attr_type {
#define f(name, tag, ...) ATTR_##tag,
    ATTR_LIST(f)
#undef f
};

// Standard attributes that are guaranteed to be present in any valid geometry.
#define STANDARD_ATTR_LIST(f) \
    f(POSITION,        VEC3, VERTEX) \
    f(SHADING_NORMAL,  VEC3, VERTEX) \
    f(GEOMETRY_NORMAL, VEC3, FACE) \
    f(MATERIAL_INDEX,  UINT, FACE) \

enum standard_attr_index {
#define f(name, ...) ATTR_##name,
    STANDARD_ATTR_LIST(f)
#undef f
};

static inline size_t get_attr_size(enum attr_type type) {
    switch (type) {
#define f(name, tag, type) case ATTR_##tag: return sizeof(type);
        ATTR_LIST(f)
#undef f
        default:
            assert(false);
            return 0;
    }
}

#endif
