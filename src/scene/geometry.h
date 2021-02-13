#ifndef SCENE_GEOMETRY_H
#define SCENE_GEOMETRY_H

#include <stdbool.h>

#include "scene/scene.h"
#include "scene/mesh.h"
#include "core/ray.h"
#include "core/vec4.h"
#include "core/vec3.h"

/*
 * These objects are intersectable geometric objects that
 * make up the scene contents. The surface of a geometric
 * object is also samplable.
 */

struct mesh;
struct thread_pool;

struct surface_sample {
    struct vec3 point;

    // Probability density function value for that point
    // (for instance, if uniform sampling is used, this value is `1/surface area`).
    real_t pdf;
};

typedef const struct geometry* geometry_t;

geometry_t new_mesh_geometry(struct scene*, const struct mesh*);
geometry_t new_submesh_geometry(struct scene*, const struct mesh*, size_t begin, size_t end);

// Prepares the given geometric object for rendering (creates BVHs, ...).
// May be computationally intensive, which is why a thread pool is provided.
void prepare_geometry(geometry_t geometry, struct thread_pool* thread_pool);

// Intersects the given geometry with the given ray.
// The `any` parameter selects the intersection mode between any and closest intersection.
bool intersect_ray_geometry(struct ray* ray, struct hit* hit, geometry_t geometry, bool any);

// Obtains an attribute from a geometry, given a ray and a hit.
union attr get_geometry_attr(geometry_t geometry, unsigned attr_index, const struct ray* ray, const struct hit* hit);

// Samples the surface of a geometry, using the provided surface coordinates (in `[0, 1]`).
struct surface_sample sample_geometry_surface(geometry_t, const struct vec2*);

real_t get_geometry_surface_area(geometry_t);

#endif
