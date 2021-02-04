#ifndef CORE_ALLOC_H
#define CORE_ALLOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

void* xmalloc(size_t);
void* xrealloc(void*, size_t);
void* xcalloc(size_t, size_t);

#endif
