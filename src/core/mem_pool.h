#ifndef CORE_MEM_POOL_H
#define CORE_MEM_POOL_H

#include <stddef.h>

struct mem_pool* new_mem_pool_with_cap(size_t cap);
struct mem_pool* new_mem_pool(void);
void free_mem_pool(struct mem_pool*);
void* alloc_from_pool(struct mem_pool** pool, size_t size);
void reset_mem_pool(struct mem_pool** pool);

#endif
