#ifndef CORE_MAT4_H
#define CORE_MAT4_H

#include "core/vec4.h"

// Column-major 4x4 matrix
struct mat4 {
    real_t _[4][4];
};

static inline struct mat4 make_mat4(struct vec4 c0, struct vec4 c1, struct vec4 c2, struct vec4 c3) {
    return (struct mat4) {
        {
            { c0._[0], c0._[1], c0._[2], c0._[3] },
            { c1._[0], c1._[1], c1._[2], c1._[3] },
            { c2._[0], c2._[1], c2._[2], c2._[3] },
            { c3._[0], c3._[1], c3._[2], c3._[3] }
        }
    };
}

static inline struct mat4 const_mat4(real_t x) {
    return make_mat4(const_vec4(x), const_vec4(x), const_vec4(x), const_vec4(x));
}

static inline struct mat4 diag_mat4(struct vec4 d) {
    return (struct mat4) { { [0][0] = d._[0], [1][1] = d._[1], [2][2] = d._[2], [3][3] = d._[3] } };
}

static inline struct mat4 add_mat4(struct mat4 a, struct mat4 b) {
    struct mat4 c;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j)
            c._[i][j] = a._[i][j] + b._[i][j];
    }
    return c;
}

static inline struct mat4 sub_mat4(struct mat4 a, struct mat4 b) {
    struct mat4 c;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j)
            c._[i][j] = a._[i][j] - b._[i][j];
    }
    return c;
}

static inline struct mat4 mul_mat4(struct mat4 a, struct mat4 b) {
    struct mat4 c = { 0 };
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 4; ++k) 
                c._[i][j] = fast_mul_add(a._[k][j], b._[i][k], c._[i][j]);
        }
    }
    return c;
}

static inline struct vec4 mul_mat4_vec4(struct mat4 a, struct vec4 b) {
    struct vec4 c = { 0 };
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j)
            c._[i] = fast_mul_add(a._[j][i], b._[j], c._[i]);
    }
    return c;
}

#endif
