#ifndef CORE_MEM_POOL_H
#define CORE_MEM_POOL_H

#include <stddef.h>

struct mem_pool;

/* Allocates a memory pool with the given initial capacity, in bytes. */
struct mem_pool* new_mem_pool_with_cap(size_t cap);
struct mem_pool* new_mem_pool(void);
void free_mem_pool(struct mem_pool* mem_pool);

/* This returns the amount of memory used in the pool. */
size_t get_used_mem(const struct mem_pool* mem_pool);

void* alloc_from_pool(struct mem_pool** mem_pool, size_t size);

/* Resets the memory pool to the given state, or to its initial state if `target_used_mem == 0`. */
void reset_mem_pool(struct mem_pool** mem_pool, size_t target_used_mem);

#endif
