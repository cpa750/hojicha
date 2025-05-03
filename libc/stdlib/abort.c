#include <stdio.h>
#include <stdlib.h>

__attribute__((__noreturn__)) void abort(void) {
#if defined(__is_libk)
  printf("Kernel panic, abort.\n");
  __asm__ __volatile__("hlt");
  // TODO add proper kernel panic
#else
  // TODO SIGINT signal
  printf("Abort.\n");
#endif
  while (1) {
  }
  __builtin_unreachable();
}

