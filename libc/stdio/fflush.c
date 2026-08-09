#include <errno.h>
#include <internal/__stdio.h>
#include <stddef.h>
#include <stdio.h>

int fflush(FILE* stream) {
  if (stream == NULL) { return 0; }
  if (stream->fd < 0) {
    stream->flags |= __HOJICHA_FILE_ERROR;
    errno = EBADF;
    return EOF;
  }
  return 0;
}
