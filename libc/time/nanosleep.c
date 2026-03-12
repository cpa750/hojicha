#include <sys/__syscalls.h>
#include <time.h>

unsigned long nanosleep(unsigned long ns) {
  __syscall1(__HOJICHA_SYS_SYSCALL_NANOSLEEP, ns);
  return ns;
}

