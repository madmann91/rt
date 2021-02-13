#ifndef SCENE_BSDF_H
#define SCENE_BSDF_H

#include "scene/scene.h"
#include "scene/geometry.h"
#include "scene/texture.h"
#include "core/vec3.h"
#include "core/rgb.h"
#include "core/mat3.h"

/*
 * BSDFs: Bi-directional Scattering Distribution Functions.
 * From the point of view of a renderer, these are black boxes
 * that can be evaluated, and sampled.
 */

typedef const struct bsdf* bsdf_t;

struct bsdf_sample {
    struct vec3 dir;
    struct rgb color;
    real_t pdf, cos;
};

struct surface_info {
    struct mat3 basis;
};

bsdf_t new_diffuse_bsdf(struct scene* scene, texture_t kd);
bsdf_t new_phong_bsdf(struct scene* scene, texture_t ks, texture_t ns);
bsdf_t new_mirror_bsdf(struct scene* scene, texture_t ks);
bsdf_t new_glass_bsdf(struct scene* scene, texture_t ks, texture_t eta);

struct rgb evaluate_bsdf(
    bsdf_t bsdf,
    const struct vec3* from,
    const struct surface_info* surface_info,
    const struct vec3* to);

struct bsdf_sample sample_bsdf(
    bsdf_t bsdf,
    const struct vec3* from,
    const struct surface_info* surface_info);

real_t evaluate_bsdf_sampling_pdf(
    bsdf_t bsdf,
    const struct vec3* from,
    const struct surface_info* surface_info,
    const struct vec3* to);

#endif
