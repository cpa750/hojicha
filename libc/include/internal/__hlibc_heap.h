#ifndef __HOJICHA_INTERNAL_HLIBC_HEAP_H
#define __HOJICHA_INTERNAL_HLIBC_HEAP_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

int __hlibc_brk(void* addr);
void* __hlibc_sbrk(intptr_t offset);

void __hlibc_heap_init(void);

#ifdef __cplusplus
}
#endif

#endif  // __HOJICHA_INTERNAL_HLIBC_HEAP_H

