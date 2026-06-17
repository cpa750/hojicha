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
  if (read(stream->fd, &c, sizeof(c)) != (int)sizeof(c)) { return EOF; }
  return c;
#endif
}
