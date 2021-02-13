#ifndef CORE_RANDOM_H
#define CORE_RANDOM_H

#include <core/config.h>
#include <core/vec2.h>
#include <core/vec3.h>
#include <core/hash.h>

#include <pcg_basic.h>

struct rnd_gen {
    pcg32_random_t rng;
};

// Creates a random generator with the given seed and sequence index.
static inline struct rnd_gen make_rnd_gen(uint64_t seed) {
    struct rnd_gen rnd_gen;
    pcg32_srandom_r(&rnd_gen.rng, seed, 0);
    return rnd_gen;
}

// Generates a random seed suitable for a random generator,
// based on the pixel coordinates and the frame index.
static inline uint64_t random_seed(size_t x, size_t y, size_t frame_index) {
    uint64_t low  = hash_uint(hash_uint(hash_init(), x), frame_index);
    uint64_t high = hash_uint(hash_uint(hash_init(), frame_index), y);
    return low | (high << 32);
}

static inline bits_t random_bits(struct rnd_gen* rnd_gen) {
#ifdef USE_DOUBLE_PRECISION
    bits_t low  = pcg32_random_r(&rnd_gen->rng);
    bits_t high = pcg32_random_r(&rnd_gen->rng);
    return low | (high << 32);
#else
    return pcg32_random_r(&rnd_gen->rng);
#endif
}

static inline real_t random_real(struct rnd_gen* rnd_gen, real_t min, real_t max) {
    // Avoid the division here by precomputing the inverse
    real_t scale = (max - min) * ((real_t)1 / (real_t)BITS_MAX);
    return clamp_real(fast_mul_add(random_bits(rnd_gen), scale, min), min, max);
}

static inline struct vec2 random_vec2(struct rnd_gen* rnd_gen, real_t min, real_t max) {
    return (struct vec2) {
        {
            random_real(rnd_gen, min, max),
            random_real(rnd_gen, min, max)
        }
    };
}

static inline struct vec3 random_vec3(struct rnd_gen* rnd_gen, real_t min, real_t max) {
    return (struct vec3) {
        {
            random_real(rnd_gen, min, max),
            random_real(rnd_gen, min, max),
            random_real(rnd_gen, min, max)
        }
    };
}

static inline real_t random_real_01(struct rnd_gen* rnd_gen) {
    return random_real(rnd_gen, 0, 1);
}

static inline struct vec2 random_vec2_01(struct rnd_gen* rnd_gen) {
    return random_vec2(rnd_gen, 0, 1);
}

static inline struct vec3 random_vec3_01(struct rnd_gen* rnd_gen) {
    return random_vec3(rnd_gen, 0, 1);
}

#endif
