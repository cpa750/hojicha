#include <errno.h>
#include <fcntl.h>
#include <internal/__syscalls.h>

int read(long fd, void* buf, long count) {
  int ret = __syscall3(__HOJICHA_INTERNAL_SYSCALL_READ, fd, (long)buf, count);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}

