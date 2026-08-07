#include <errno.h>
#include <internal/__syscalls.h>
#include <time.h>

int clock_gettime(clockid_t clockid, struct timespec* tp) {
  int ret = __syscall2(
      __HOJICHA_INTERNAL_SYSCALL_CLOCK_GETTIME, clockid, (long)tp);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
