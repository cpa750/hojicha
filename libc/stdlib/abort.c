#include <stdio.h>
#include <stdlib.h>
#if defined(__is_libk)
#include <kernel/kernel_state.h>
#endif

__attribute__((__noreturn__)) void abort(void) {
#if defined(__is_libk)
  printf("Kernel panic, abort.\n");
  printf("Dumping g_kernel:\n");
  g_kernel_dump();
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

