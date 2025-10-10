#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

void kmalloc_initialize();
void* kmalloc(size_t size);
void kfree(void* ptr);
void kmalloc_print_free_blocks();

#endif  // KMALLOC_H

