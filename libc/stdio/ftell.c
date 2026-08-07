#include <errno.h>
#include <internal/__stdio.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

long ftell(FILE* stream) {
  if (stream == NULL) {
    errno = EINVAL;
    return -1;
  }

#if defined(__is_libk)
  stream->flags |= __HOJICHA_FILE_ERROR;
  errno = ENOSYS;
  return -1;
#else
  long ret = lseek(stream->fd, 0, SEEK_CUR);
  if (ret < 0) { stream->flags |= __HOJICHA_FILE_ERROR; }
  return ret;
#endif
}
