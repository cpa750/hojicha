#include <stdio.h>

#if defined(__is_libk)
#include <kernel/tty.h>
#endif

int putchar(int char_as_int) {
#if defined(__is_libk)
  char c = (char)char_as_int;
  terminal_write(&c, sizeof(c));
#else
  // TODO implement print to stdout
#endif
  return char_as_int;
}

