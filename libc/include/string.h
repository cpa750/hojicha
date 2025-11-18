#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

int memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);
void* strcpy(void* restrict, const void* retrict);
size_t strlen(const char*);

#ifdef __cplusplus
}
#endif

#endif
