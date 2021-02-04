#ifndef CORE_BBOX_H
#define CORE_BBOX_H

#include "core/vec3.h"

struct bbox {
    struct vec3 min, max;
};

static inline struct bbox extend_bbox(struct bbox bbox, struct vec3 point) {
    return (struct bbox) {
        .min = min_vec3(bbox.min, point),
        .max = max_vec3(bbox.max, point)
    };
}

static inline struct bbox union_bbox(struct bbox a, struct bbox b) {
    return (struct bbox) {
        .min = min_vec3(a.min, b.min),
        .max = max_vec3(a.max, b.max)
    };
}

static inline real_t half_bbox_area(struct bbox bbox) {
    struct vec3 e = max_vec3(sub_vec3(bbox.max, bbox.min), (struct vec3) { { 0, 0, 0 } });
    return (e._[0] + e._[1]) * e._[2] + e._[0] * e._[1];
}

static inline struct bbox point_bbox(struct vec3 p) {
    return (struct bbox) { .min = p, .max = p };
}

static inline struct bbox empty_bbox(void) {
    return (struct bbox) {
        .min = (struct vec3) { {  REAL_MAX,  REAL_MAX,  REAL_MAX } },
        .max = (struct vec3) { { -REAL_MAX, -REAL_MAX, -REAL_MAX } }
    };
}

#endif
