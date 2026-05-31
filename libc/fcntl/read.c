#include <errno.h>
#include <fcntl.h>
#include <sys/__syscalls.h>

int read(long fd, void* buf, long count) {
  int ret = __syscall3(__HOJICHA_SYS_SYSCALL_READ, fd, (long)buf, count);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}

