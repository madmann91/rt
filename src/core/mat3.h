#ifndef CORE_MAT3_H
#define CORE_MAT3_H

#include "core/vec3.h"

// Column-major 3x3 matrix
struct mat3 {
    real_t _[3][3];
};

static inline struct mat3 make_mat3(struct vec3 c0, struct vec3 c1, struct vec3 c2) {
    return (struct mat3) {
        {
            { c0._[0], c0._[1], c0._[2] },
            { c1._[0], c1._[1], c1._[2] },
            { c2._[0], c2._[1], c2._[2] }
        }
    };
}

static inline struct mat3 const_mat3(real_t x) {
    return make_mat3(const_vec3(x), const_vec3(x), const_vec3(x));
}

static inline struct mat3 diag_mat3(struct vec3 d) {
    return (struct mat3) { { [0][0] = d._[0], [1][1] = d._[1], [2][2] = d._[2] } };
}

static inline struct mat3 add_mat3(struct mat3 a, struct mat3 b) {
    struct mat3 c;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j)
            c._[i][j] = a._[i][j] + b._[i][j];
    }
    return c;
}

static inline struct mat3 sub_mat3(struct mat3 a, struct mat3 b) {
    struct mat3 c;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j)
            c._[i][j] = a._[i][j] - b._[i][j];
    }
    return c;
}

static inline struct mat3 mul_mat3(struct mat3 a, struct mat3 b) {
    struct mat3 c = { 0 };
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) 
                c._[i][j] = fast_mul_add(a._[k][j], b._[i][k], c._[i][j]);
        }
    }
    return c;
}

static inline struct vec3 mul_mat3_vec3(struct mat3 a, struct vec3 b) {
    struct vec3 c = { 0 };
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j)
            c._[i] = fast_mul_add(a._[j][i], b._[j], c._[i]);
    }
    return c;
}

#endif
