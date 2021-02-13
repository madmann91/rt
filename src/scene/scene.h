#ifndef SCENE_SCENE_H
#define SCENE_SCENE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * The scene is a special container that manages node
 * creation and destruction. Every scene node has a hash
 * and compare function that is used to hash-cons them.
 * This means that if a node with the same parameters is
 * already found in the scene, then that node is returned,
 * instead of creating a new one.
 */

struct mem_pool;
struct hash_table;

struct scene_node {
    enum scene_node_type {
        SUBMESH_GEOMETRY,
        GROUP_GEOMETRY,
        MATERIAL,
        DIFFUSE_BSDF,
        POINT_LIGHT,
        AREA_LIGHT,
    } type;

    uint32_t (*hash)(const struct scene_node*);
    bool (*compare)(const struct scene_node*, const struct scene_node*);

    // Cleanup function that will be called once the scene is freed.
    // Can be set to `NULL` if the node does not need cleanup.
    void (*cleanup)(struct scene_node*);
};

struct scene {
    struct mem_pool* mem_pool;
    struct hash_table* node_table;
};

struct scene* new_scene(void);
void free_scene(struct scene*);

// This function is used internally by all nodes to perform hash-consing.
// There should be no need to call it directly.
const struct scene_node* insert_scene_node(struct scene*, struct scene_node*, size_t);

#endif
