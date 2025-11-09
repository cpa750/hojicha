#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((__noreturn__)) void abort(void);
char* itoa(int num, char* dst, int base);
void* malloc(size_t size);
void free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif

