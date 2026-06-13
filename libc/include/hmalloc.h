#ifndef HOJICHA_HMALLOC_H
#define HOJICHA_HMALLOC_H

#include <stddef.h>

void* hmalloc(size_t size);
void hfree(void* ptr);

#if defined(__is_libk) || defined(__is_kernel)
void hmalloc_initialize(void);
void* hmalloc_page_aligned(size_t size);
#endif

#endif  // HOJICHA_HMALLOC_H
