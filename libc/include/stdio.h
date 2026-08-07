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
int fclose(FILE*);
int feof(FILE*);
int fflush(FILE*);
int ferror(FILE*);
char* fgets(char* restrict, int, FILE* restrict);
FILE* fopen(const char* restrict, const char* restrict);
int fputc(int, FILE*);
int fprintf(FILE* restrict, const char* restrict, ...);
size_t fread(void* restrict, size_t, size_t, FILE* restrict);
int fseek(FILE*, long, int);
long ftell(FILE*);
size_t fwrite(const void* restrict, size_t, size_t, FILE* restrict);
int getchar(void);
int printf(const char* restrict, ...);
int putchar(int);
int puts(const char*);
int snprintf(char* restrict buffer, size_t size, const char* restrict format, ...);
int remove(const char*);
int rename(const char*, const char*);
int sscanf(const char* restrict, const char* restrict, ...);
int vfprintf(FILE* restrict, const char* restrict, va_list);
int vprintf(const char* restrict format, va_list parameters);
int vsnprintf(char* restrict buffer,
              size_t size,
              const char* restrict format,
              va_list parameters);

#ifdef __cplusplus
}
#endif

#endif
