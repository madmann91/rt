#ifndef CORE_BBOX_H
#define CORE_BBOX_H

#include "core/vec3.h"
#include "core/utils.h"

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
    return fast_mul_add(e._[0], e._[1], fast_mul_add(e._[0], e._[2], e._[1] * e._[2]));
}

static inline bool bbox_contains(const struct bbox bbox, const struct bbox other) {
    return
        bbox.max._[0] >= other.max._[0] && bbox.min._[0] <= other.min._[0] &&
        bbox.max._[1] >= other.max._[1] && bbox.min._[1] <= other.min._[1] &&
        bbox.max._[2] >= other.max._[2] && bbox.min._[2] <= other.min._[2];
}

static inline bool bbox_overlaps(const struct bbox bbox, const struct bbox other) {
    return
        bbox.max._[0] >= other.min._[0] && bbox.min._[0] <= other.max._[0] &&
        bbox.max._[1] >= other.min._[1] && bbox.min._[1] <= other.max._[1] &&
        bbox.max._[2] >= other.min._[2] && bbox.min._[2] <= other.max._[2];
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
