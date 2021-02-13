#include <stdio.h>
#include <assert.h>

#include "scene/scene.h"
#include "core/mem_pool.h"
#include "core/hash_table.h"

struct scene* new_scene(void) {
    struct scene* scene = xmalloc(sizeof(struct scene));
    scene->mem_pool = new_mem_pool();
    scene->node_table = new_hash_table(sizeof(struct scene_node*), sizeof(struct scene_node*));
    return scene;
}

static bool compare_scene_nodes(const void* left, const void* right) {
    const struct scene_node* left_node  = *(const struct scene_node**)left;
    const struct scene_node* right_node = *(const struct scene_node**)right;
    return left_node->type == right_node->type && left_node->compare(left_node, right_node);
}

const struct scene_node* insert_scene_node(struct scene* scene, struct scene_node* node, size_t size) {
    assert(node->hash && node->compare);
    uint32_t hash = node->hash(node);
    size_t node_index = find_in_hash_table(
        scene->node_table,
        &node, sizeof(struct scene_node*),
        hash, compare_scene_nodes);
    if (node_index != SIZE_MAX)
        return ((const struct scene_node**)scene->node_table->values)[node_index];
    struct scene_node* node_copy = alloc_from_pool(&scene->mem_pool, size);
    // TODO: Simplify nodes to speed up rendering
    struct scene_node* simplified_node = node_copy;
    memcpy(node_copy, node, size);
    insert_in_hash_table(
        scene->node_table,
        &node_copy, sizeof(struct scene_node*),
        &simplified_node, sizeof(struct scene_node*),
        hash, compare_scene_nodes);
    return node_copy;
}

void free_scene(struct scene* scene) {
    for (size_t i = 0, n = scene->node_table->cap; i < n; ++i) {
        if (!is_bucket_occupied(scene->node_table, i))
            continue;
        struct scene_node* node = ((struct scene_node**)scene->node_table->values)[i];
        if (node->cleanup)
            node->cleanup(node);
    }
    free_mem_pool(scene->mem_pool);
    free_hash_table(scene->node_table);
    free(scene);
}
