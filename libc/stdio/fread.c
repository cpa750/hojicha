#include <errno.h>
#include <fcntl.h>
#include <internal/__stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

size_t fread(void* restrict ptr,
             size_t size,
             size_t nmemb,
             FILE* restrict stream) {
  if (size == 0 || nmemb == 0) { return 0; }
  if (ptr == NULL || stream == NULL || size > SIZE_MAX / nmemb) {
    if (stream != NULL) { stream->flags |= __HOJICHA_FILE_ERROR; }
    errno = EINVAL;
    return 0;
  }

#if defined(__is_libk)
  stream->flags |= __HOJICHA_FILE_ERROR;
  errno = ENOSYS;
  return 0;
#else
  size_t total = size * nmemb;
  size_t bytes_read = 0;

  while (bytes_read < total) {
    int ret =
        read(stream->fd, (char*)ptr + bytes_read, (long)(total - bytes_read));
    if (ret < 0) {
      stream->flags |= __HOJICHA_FILE_ERROR;
      break;
    }
    if (ret == 0) {
      stream->flags |= __HOJICHA_FILE_EOF;
      break;
    }
    bytes_read += (size_t)ret;
  }

  return bytes_read / size;
#endif
}
