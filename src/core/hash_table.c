#include <string.h>
#include <assert.h>

#include "core/hash_table.h"
#include "core/utils.h"

#define DEFAULT_CAP 8

static inline size_t increment_wrap(size_t cap, size_t index) {
    return index + 1 >= cap ? 0 : index + 1;
}

static inline bool needs_rehash(const struct hash_table* hash_table) {
    return hash_table->size * 100 > hash_table->cap * MAX_LOAD_FACTOR;
}

struct hash_table* new_hash_table_with_cap(size_t key_size, size_t value_size, size_t cap) {
    struct hash_table* hash_table = xmalloc(sizeof(struct hash_table));
    cap = next_prime(cap);
    hash_table->cap = cap;
    hash_table->size = 0;
    hash_table->keys = xmalloc(key_size * cap);
    hash_table->values = xmalloc(value_size * cap);
    hash_table->hashes = xcalloc(cap, sizeof(uint32_t));
    return hash_table;
}

struct hash_table* new_hash_table(size_t key_size, size_t value_size) {
    return new_hash_table_with_cap(key_size, value_size, DEFAULT_CAP);
}

void free_hash_table(struct hash_table* hash_table) {
    free(hash_table->keys);
    free(hash_table->values);
    free(hash_table->hashes);
    free(hash_table);
}

static void rehash(struct hash_table* hash_table, size_t key_size, size_t value_size) {
    size_t new_cap = next_prime(hash_table->cap);
    if (new_cap <= hash_table->cap)
        new_cap = hash_table->cap * 2 - 1;
    void* new_keys       = xmalloc(key_size * new_cap);
    void* new_values     = xmalloc(value_size * new_cap);
    uint32_t* new_hashes = xcalloc(new_cap, sizeof(uint32_t));
    for (size_t i = 0, n = hash_table->cap; i < n; ++i) {
        uint32_t hash = hash_table->hashes[i];
        if ((hash & ~HASH_MASK) == 0)
            continue;
        size_t index = mod_prime(hash, new_cap);
        while (new_hashes[index] & ~HASH_MASK)
            index = increment_wrap(new_cap, index);
        const void* key   = ((char*)hash_table->keys) + key_size * i;
        const void* value = ((char*)hash_table->values) + value_size * i;
        memcpy(((char*)new_keys) + key_size * index, key, key_size);
        memcpy(((char*)new_values) + value_size * index, value, value_size);
        new_hashes[index] = hash;
    }
    free(hash_table->keys);
    free(hash_table->hashes);
    free(hash_table->values);
    hash_table->keys   = new_keys;
    hash_table->values = new_values;
    hash_table->hashes = new_hashes;
    hash_table->cap    = new_cap;
}

bool insert_in_hash_table(
    struct hash_table* hash_table,
    const void* key, size_t key_size,
    const void* value, size_t value_size,
    uint32_t hash, compare_fn_t compare)
{
    hash |= ~HASH_MASK;
    size_t index = mod_prime(hash, hash_table->cap);
    while (is_bucket_occupied(hash_table, index)) {
        if (hash_table->hashes[index] == hash &&
            compare(((char*)hash_table->keys) + key_size * index, key))
            return false;
        index = increment_wrap(hash_table->cap, index);
    }
    memcpy(((char*)hash_table->keys) + key_size * index, key, key_size);
    memcpy(((char*)hash_table->values) + value_size * index, value, value_size);
    hash_table->hashes[index] = hash;
    hash_table->size++;
    if (needs_rehash(hash_table))
        rehash(hash_table, key_size, value_size);
    return true;
}

size_t find_in_hash_table(
    const struct hash_table* hash_table,
    const void* key, size_t key_size,
    uint32_t hash, compare_fn_t compare)
{
    hash |= ~HASH_MASK;
    size_t index = mod_prime(hash, hash_table->cap);
    while (is_bucket_occupied(hash_table, index)) {
        if (hash_table->hashes[index] == hash &&
            compare(((char*)hash_table->keys) + key_size * index, key))
            return index;
        index = increment_wrap(hash_table->cap, index);
    }
    return SIZE_MAX;
}

bool remove_from_hash_table(
    struct hash_table* hash_table, size_t index,
    size_t key_size, size_t value_size)
{
    size_t next_index = increment_wrap(hash_table->cap, index);
    void* key   = ((char*)hash_table->keys)   + key_size * index;
    void* value = ((char*)hash_table->values) + value_size * index;

    // Move the elements that belong to the collision chain
    while (is_bucket_occupied(hash_table, next_index)) {
        uint32_t next_hash = hash_table->hashes[next_index];
        size_t desired_index = mod_prime(next_hash, hash_table->cap);
        if (next_index == desired_index)
            break;
        void* next_key   = ((char*)hash_table->keys)   + key_size * next_index;
        void* next_value = ((char*)hash_table->values) + value_size * next_index;
        memcpy(key, next_key, key_size);
        memcpy(value, next_value, value_size);
        hash_table->hashes[index] = next_hash;
        key   = next_key;
        value = next_value;
        index = next_index;
        next_index = increment_wrap(hash_table->cap, next_index);
    }

    hash_table->hashes[index] = 0;
    hash_table->size--;
    return true;
}

void clear_hash_table(struct hash_table* hash_table) {
    memset(hash_table->hashes, 0, sizeof(uint32_t) * hash_table->cap);
    hash_table->size = 0;
}
