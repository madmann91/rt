#ifndef SCENE_MATERIAL_H
#define SCENE_MATERIAL_H

#include "scene/scene.h"
#include "scene/bsdf.h"

/*
 * Materials: The combination of a BSDF that represents the material
 * reflection, along with an emitter, which represents the material
 * emission properties.
 */

typedef const struct material* material_t;

struct material {
    bsdf_t bsdf;
    struct emitter {
        geometry_t geometry;
        struct rgb intensity;
    } emitter;
};

struct material* new_material(struct scene* scene, bsdf_t bsdf);
struct material* new_emissive_material(
    struct scene* scene, bsdf_t bsdf,
    const struct emitter* emitter);

#endif
