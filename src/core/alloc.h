#ifndef CORE_ALLOC_H
#define CORE_ALLOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

void* xmalloc(size_t);
void* xrealloc(void*, size_t);
void* xcalloc(size_t, size_t);

#define SMALL_BUF_SIZE 32 
#define SMALL_BUF_STRUCT(T) \
    struct { bool on_stack; alignas(max_align_t) T elems[SMALL_BUF_SIZE]; }

struct large_buf {
    bool on_stack;
    max_align_t elems[];
};

#define new_buf(T, size) \
    ((size) < SMALL_BUF_SIZE \
     ? (T*)(&((SMALL_BUF_STRUCT(T)){ .on_stack = true }))->elems \
     : (T*)new_large_buf((size) * sizeof(T)))

static inline void* new_large_buf(size_t size) {
    struct large_buf* large_buf = xmalloc(sizeof(struct large_buf) + size);
    large_buf->on_stack = false;
    return large_buf->elems;
}

static inline void free_buf(void* buf) {
    void* ptr = ((char*)buf) - offsetof(struct { bool on_stack; max_align_t elems[]; }, elems);
    if (!*(bool*)ptr)
        free(ptr);
}

#endif
