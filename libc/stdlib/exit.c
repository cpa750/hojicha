#include <stdlib.h>
#include <sys/__syscalls.h>

__attribute__((__noreturn__)) void exit(int code) {
  __syscall1(__HOJICHA_SYS_SYSCALL_EXIT, code);
  for (;;) {
    asm volatile("pause");
  }
}
