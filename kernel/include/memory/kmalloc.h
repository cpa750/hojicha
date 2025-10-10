#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

void kmalloc_initialize();
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif  // KMALLOC_H

