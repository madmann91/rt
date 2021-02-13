#include "bsdf.h"

// TODO
struct bsdf {
    struct scene_node scene_node;

    struct rgb (*evaluate)(
        bsdf_t bsdf,
        const struct vec3* from,
        const struct surface_info* surface_info,
        const struct vec3* to);
    struct bsdf_sample (*sample)(
        bsdf_t bsdf,
        const struct vec3* from,
        const struct surface_info* surface_info);
    real_t (*evaluate_sampling_pdf)(
        bsdf_t bsdf,
        const struct vec3* from,
        const struct surface_info* surface_info,
        const struct vec3* to);
};
