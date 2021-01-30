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
    while (mem_pool) {
        struct mem_pool* prev = mem_pool->prev;
        free(mem_pool);
        mem_pool = prev;
    }
}

void* alloc_from_pool(struct mem_pool** root, size_t size) {
    struct mem_pool* pool = *root;
    while (true) {
        if (pool->cap - pool->size >= size) {
            // Align to the maximum alignment requirement
            void*  ptr = pool->data + pool->size;
            size_t pad = size % sizeof(max_align_t);
            pool->size += size + (pad == 0 ? 0 : sizeof(max_align_t) - pad);
            *root = pool;
            return ptr;
        }

        if (pool->next)
            pool = pool->next;
        else {
            struct mem_pool* next = new_mem_pool_with_cap(size > pool->cap ? size : pool->cap);
            next->prev = pool;
            pool = next;
        }
    }
}

void reset_mem_pool(struct mem_pool** root) {
    struct mem_pool* pool = *root;
    struct mem_pool* first = pool;
    while (pool) {
        pool->size = 0;
        first = pool;
        pool = pool->prev;
    }
    *root = first;
}
