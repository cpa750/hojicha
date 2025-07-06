#ifndef STDLIB_H
#define STDLIB_H

#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((__noreturn__)) void abort(void);
char* itoa(int num, char* dst, int base);

#ifdef __cplusplus
}
#endif

#endif

