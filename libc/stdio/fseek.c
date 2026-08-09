#include <errno.h>
#include <internal/__stdio.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

int fseek(FILE* stream, long offset, int whence) {
  if (stream == NULL) {
    errno = EINVAL;
    return -1;
  }

#if defined(__is_libk)
  (void)offset;
  (void)whence;
  stream->flags |= __HOJICHA_FILE_ERROR;
  errno = ENOSYS;
  return -1;
#else
  if (lseek(stream->fd, offset, whence) < 0) {
    stream->flags |= __HOJICHA_FILE_ERROR;
    return -1;
  }
  stream->flags &= ~__HOJICHA_FILE_EOF;
  return 0;
#endif
}
