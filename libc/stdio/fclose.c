#include <errno.h>
#include <internal/__stdio.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int fclose(FILE* stream) {
  if (stream == NULL) {
    errno = EINVAL;
    return EOF;
  }

#if defined(__is_libk)
  errno = ENOSYS;
  return EOF;
#else
  int ret = close(stream->fd);
  stream->fd = -1;
  if (stream->flags & __HOJICHA_FILE_OWNED) { free(stream); }
  return ret < 0 ? EOF : 0;
#endif
}
