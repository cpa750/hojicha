#include <errno.h>
#include <fcntl.h>
#include <internal/__syscalls.h>

int open(const char* path, int flags, int mode) {
  int ret = __syscall2(__HOJICHA_INTERNAL_SYSCALL_OPEN, (long)path, (long)flags);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}

