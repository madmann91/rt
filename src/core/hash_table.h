#ifndef CORE_HASH_TABLE_H
#define CORE_HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "core/utils.h"
#include "core/primes.h"
#include "core/hash.h"

/*
 * This table only uses the lower 31 bits of the hash value.
 * The highest bit is used to encode buckets that are used.
 * Hashes are stored in the hash map to speed up comparisons:
 * The hash value is compared with the bucket's hash value first,
 * and the comparison function is only used if they compare equal.
 * The collision resolution strategy is linear probing.
 */

#define HASH_MASK UINT32_C(0x7FFFFFFF)
#define MAX_LOAD_FACTOR 70//%

#define GEN_DEFAULT_HASH(name, T) \
    static inline uint32_t hash_##name(const T* key) { \
        return hash_bytes(hash_init(), key, sizeof(T)); \
    }

#define GEN_DEFAULT_COMPARE(name, T) \
    static bool compare_##name(const void* left, const void* right) { \
        return !memcmp(left, right, sizeof(T)); \
    }

struct hash_table {
    size_t cap;
    size_t size;
    uint32_t* hashes;
    void* keys;
    void* values;
};

static inline bool is_bucket_occupied(const struct hash_table* hash_table, size_t index) {
    return (hash_table->hashes[index] & ~HASH_MASK) != 0;
}

typedef bool (*compare_fn_t)(const void*, const void*);

// Creates a hash table with the given key and value size.
// A value size of 0 is accepted and means that the hash table is a set, not a map.
struct hash_table* new_hash_table_with_cap(size_t key_size, size_t value_size, size_t cap);
// Same as above, but with a default capacity.
struct hash_table* new_hash_table(size_t key_size, size_t value_size);

void free_hash_table(struct hash_table*);

// Inserts an element in the table.
// If the element already exists, the function does nothing and returns false.
bool insert_in_hash_table(
    struct hash_table* hash_table,
    const void* key, size_t key_size,
    const void* value, size_t value_size,
    uint32_t hash, compare_fn_t compare);

// Finds an element in the hash table. Returns `SIZE_MAX` if the element cannot be found.
size_t find_in_hash_table(
    const struct hash_table*,
    const void* key, size_t key_size,
    uint32_t hash, compare_fn_t compare);

bool remove_from_hash_table(
    struct hash_table* hash_table, size_t index,
    size_t key_size, size_t value_size);

void clear_htable(struct hash_table*);

#endif
