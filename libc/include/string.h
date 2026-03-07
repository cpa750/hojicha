#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

int memcmp(const void* a, const void* b, size_t len);
void* memcpy(void* __restrict dst, const void* __restrict src, size_t len);
void* memmove(void* dst, const void* src, size_t len);
void* memset(void* buf, int val, size_t len);
int strcmp(const char* a, const char* b);
void* strcpy(void* restrict, const void* restrict s);
size_t strlen(const char* str);

#ifdef __cplusplus
}
#endif

#endif
