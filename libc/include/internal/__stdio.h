#ifndef __HOJICHA_INTERNAL_STDIO_H
#define __HOJICHA_INTERNAL_STDIO_H

#include <stdio.h>

struct __hojicha_file {
  int fd;
  int flags;
};

#define __HOJICHA_FILE_EOF   1
#define __HOJICHA_FILE_ERROR 2
#define __HOJICHA_FILE_OWNED 4

#endif  // __HOJICHA_INTERNAL_STDIO_H
