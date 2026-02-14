#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <sys/cdefs.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char* restrict, ...);
int putchar(int);
int puts(const char*);
int vprintf(const char* restrict format, va_list parameters);
int vsnprintf(char* buffer, const char* format, va_list parameters);

#ifdef __cplusplus
}
#endif

#endif
