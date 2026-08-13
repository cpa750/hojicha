#ifndef __HOJICHA_INTERNAL_STDIO_H
#define __HOJICHA_INTERNAL_STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

struct __hojicha_file {
  int fd;
  int flags;
};

#define __HOJICHA_FILE_EOF   1
#define __HOJICHA_FILE_ERROR 2
#define __HOJICHA_FILE_OWNED 4

typedef int (*__hojicha_printf_write_fn)(void* ctx,
                                         const char* data,
                                         size_t len);

int __hojicha_vformat(__hojicha_printf_write_fn write,
                      void* ctx,
                      const char* restrict format,
                      va_list parameters);

#endif  // __HOJICHA_INTERNAL_STDIO_H
