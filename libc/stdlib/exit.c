#include <stdlib.h>
#include <internal/__syscalls.h>

__attribute__((__noreturn__)) void exit(int code) {
  __syscall1(__HOJICHA_INTERNAL_SYSCALL_EXIT, code);
  __builtin_unreachable();
}
