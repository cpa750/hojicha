#include <fcntl.h>
#include <internal/__stdio.h>
#include <stddef.h>
#include <stdio.h>

int fgetc(FILE* stream) {
  if (stream == NULL) { return EOF; }

#if defined(__is_libk)
  return EOF;
#else
  unsigned char c;
  int ret = read(stream->fd, &c, sizeof(c));
  if (ret == 0) {
    stream->flags |= __HOJICHA_FILE_EOF;
    return EOF;
  }
  if (ret != (int)sizeof(c)) {
    stream->flags |= __HOJICHA_FILE_ERROR;
    return EOF;
  }
  return c;
#endif
}
