#ifndef HOJICHA_MEMORY_SLAB_H
#define HOJICHA_MEMORY_SLAB_H

#include <stddef.h>

void* slab_alloc(size_t size);
void* slab_calloc(size_t size);
void slab_free(void* ptr);

#endif  // HOJICHA_MEMORY_SLAB_H
