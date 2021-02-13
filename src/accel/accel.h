#ifndef ACCEL_ACCEL_H
#define ACCEL_ACCEL_H

#include <stdbool.h>

struct ray;
struct hit;

struct accel {
    bool (*intersect_ray)(struct ray*, struct hit*, const struct accel*, bool);
    void (*free)(struct accel*);
};

// Intersects the given ray with the acceleration data structure.
// If `any` is set, the routine stops immediately after finding an intersection.
// Otherwise (if `any == false`) it continues until it finds the closest one.
static inline bool intersect_ray_accel(
    struct ray* ray, struct hit* hit,
    const struct accel* accel, bool any)
{
    return accel->intersect_ray(ray, hit, accel, any);
}

// Function to call to release the memory used by an acceleration data structure.
static inline void free_accel(struct accel* accel) {
    accel->free(accel);
}

#endif
