#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>

#include "core/mem_pool.h"
#include "core/alloc.h"

#define DEFAULT_CAP 4096

struct mem_pool {
    size_t size, cap;
    struct mem_pool* prev, *next;
    alignas(max_align_t) char data[];
};

struct mem_pool* new_mem_pool_with_cap(size_t cap) {
    struct mem_pool* mem_pool = xmalloc(sizeof(struct mem_pool) + cap);
    mem_pool->cap = cap;
    mem_pool->size = 0;
    mem_pool->next = mem_pool->prev = NULL;
    return mem_pool;
}

struct mem_pool* new_mem_pool(void) {
    return new_mem_pool_with_cap(DEFAULT_CAP);
}

void free_mem_pool(struct mem_pool* mem_pool) {
    struct mem_pool* cur = mem_pool->next;
    while (cur) {
        struct mem_pool* next = cur->next;
        free(cur);
        cur = next;
    }
    cur = mem_pool->prev;
    while (cur) {
        struct mem_pool* prev = cur->prev;
        free(cur);
        cur = prev;
    }
    free(mem_pool);
}

size_t get_used_mem(const struct mem_pool* mem_pool) {
    size_t used_mem = mem_pool->size;
    struct mem_pool* cur = mem_pool->prev;
    while (cur) {
        used_mem += cur->size;
        cur = cur->prev;
    }
    return used_mem;
}

static inline size_t remaining_size(const struct mem_pool* mem_pool) {
    return mem_pool->cap - mem_pool->size;
}

void* alloc_from_pool(struct mem_pool** root, size_t size) {
    if (size == 0)
        return NULL;

    // Align the size to the largest alignment requirement
    size_t pad = size % sizeof(max_align_t);
    size = pad != 0 ? size + sizeof(max_align_t) - pad : size;

    // Find a block where the allocation can be made
    struct mem_pool* cur = *root;
    while (remaining_size(cur) < size) {
        if (cur->next)
            cur = cur->next;
        else {
            struct mem_pool* next = new_mem_pool_with_cap(size > cur->cap ? size : cur->cap);
            next->prev = cur;
            cur->next  = next;
            cur = next;
            break;
        }
    }

    void* ptr = cur->data + cur->size;
    cur->size += size;
    *root = cur;
    return ptr;
}

void reset_mem_pool(struct mem_pool** root, size_t target_used_mem) {
    struct mem_pool* cur = *root;
    size_t used_mem = get_used_mem(cur);
    while (cur->prev && used_mem > target_used_mem) {
        if (used_mem - target_used_mem > cur->size) {
            cur->size -= used_mem - target_used_mem;
            break;
        } else {
            used_mem -= cur->size;
            cur->size = 0;
            cur = cur->prev;
        }
    }
    *root = cur;
}
