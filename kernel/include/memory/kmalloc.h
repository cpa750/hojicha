#ifndef KMALLOC_H
#define KMALLOC_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

void kmalloc_initialize();
void* kmalloc_page_aligned(size_t size);
void* kmalloc(size_t size);
void kfree(void* ptr);

#if defined(__stress_kmalloc)
uint64_t kmalloc_debug_last_footer(void);
void* kmalloc_debug_last_block_user(void);
size_t kmalloc_debug_last_block_size(void);
bool kmalloc_debug_last_block_is_free(void);
#endif

#endif  // KMALLOC_H
