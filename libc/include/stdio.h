#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

#define EOF (-1)

typedef struct __hojicha_file FILE;

#ifdef __cplusplus
extern "C" {
#endif

extern FILE __hojicha_stdin;
extern FILE __hojicha_stdout;
extern FILE __hojicha_stderr;

#define stdin  (&__hojicha_stdin)
#define stdout (&__hojicha_stdout)
#define stderr (&__hojicha_stderr)

int fgetc(FILE*);
char* fgets(char* restrict, int, FILE* restrict);
int fputc(int, FILE*);
int getchar(void);
int printf(const char* restrict, ...);
int putchar(int);
int puts(const char*);
int vprintf(const char* restrict format, va_list parameters);
int vsnprintf(char* buffer, const char* format, va_list parameters);

#ifdef __cplusplus
}
#endif

#endif
