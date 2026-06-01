#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/__syscalls.h>

#if defined(__is_libk)
#include <drivers/tty.h>
#endif

int putchar(int char_as_int) {
  char c = (char)char_as_int;
#if defined(__is_libk)
  terminal_write(&c, sizeof(c));
#else
  write(1, &c, sizeof(c));
#endif
  return char_as_int;
}
