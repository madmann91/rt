#ifndef CORE_MAT3X4_H
#define CORE_MAT3X4_H

#include "core/vec4.h"
#include "core/vec3.h"

// Column-major 3x4 matrix.
// This is used to represent affine transformations
// (the combination of a mat3 and a translation).
struct mat4x3 {
    real_t _[4][3];
};

static inline struct mat4x3 make_mat4x3(struct vec3 c0, struct vec3 c1, struct vec3 c2, struct vec3 c3) {
    return (struct mat4x3) {
        {
            { c0._[0], c0._[1], c0._[2] },
            { c1._[0], c1._[1], c1._[2] },
            { c2._[0], c2._[1], c2._[2] },
            { c3._[0], c3._[1], c3._[2] }
        }
    };
}

static inline struct mat4x3 const_mat4x3(real_t x) {
    return make_mat4x3(const_vec3(x), const_vec3(x), const_vec3(x), const_vec3(x));
}

static inline struct mat4x3 diag_mat4x3(struct vec3 d) {
    return (struct mat4x3) { { [0][0] = d._[0], [1][1] = d._[1], [2][2] = d._[2] } };
}

static inline struct mat4x3 add_mat4x3(struct mat4x3 a, struct mat4x3 b) {
    struct mat4x3 c;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j)
            c._[i][j] = a._[i][j] + b._[i][j];
    }
    return c;
}

static inline struct mat4x3 sub_mat4x3(struct mat4x3 a, struct mat4x3 b) {
    struct mat4x3 c;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j)
            c._[i][j] = a._[i][j] - b._[i][j];
    }
    return c;
}

static inline struct vec3 mul_mat4x3_vec4(struct mat4x3 a, struct vec4 b) {
    struct vec3 c = { 0 };
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j)
            c._[i] = fast_mul_add(a._[j][i], b._[j], c._[i]);
    }
    return c;
}

#endif
