#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((__noreturn__)) void abort(void);
int abs(int j);
int atoi(const char* nptr);
#if !defined(__is_libk)
double atof(const char* nptr);
#endif
char* itoa(int64_t num, char* dst, int base);
char* utoa(uint64_t num, char* dst, int base);

void* malloc(size_t size);
void* calloc(size_t count, size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);

char* getenv(const char* name);

__attribute__((__noreturn__)) void exit(int code);

#ifdef __cplusplus
}
#endif

#endif
