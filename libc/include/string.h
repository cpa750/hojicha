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
char* strchr(const char* str, int c);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t len);
void* strcpy(void* restrict, const void* restrict s);
char* strdup(const char* str);
size_t strlen(const char* str);
char* strtok(char* restrict str, const char* restrict delim);
char* strtok_r(char* restrict str,
               const char* restrict delim,
               char** restrict saveptr);

#ifdef __cplusplus
}
#endif

#endif
