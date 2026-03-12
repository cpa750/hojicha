#include <sys/__syscalls.h>
#include <unistd.h>

#define __HOJICHA_NANOSLEEP_NS_MULTIPLIER 1000000000ULL

unsigned int sleep(unsigned int seconds) {
  __syscall1(__HOJICHA_SYS_SYSCALL_NANOSLEEP,
             seconds * __HOJICHA_NANOSLEEP_NS_MULTIPLIER);
  return seconds;
}

