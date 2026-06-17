#include <fcntl.h>
#include <internal/__stdio.h>
#include <stddef.h>
#include <stdio.h>

#if defined(__is_libk)
#include <drivers/tty.h>
#endif

int fputc(int char_as_int, FILE* stream) {
  if (stream == NULL) { return EOF; }

  char c = (char)char_as_int;
#if defined(__is_libk)
  terminal_write(&c, sizeof(c));
#else
  if (write(stream->fd, &c, sizeof(c)) != (int)sizeof(c)) { return EOF; }
#endif
  return (unsigned char)c;
}
