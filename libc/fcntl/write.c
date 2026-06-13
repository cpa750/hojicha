#include <errno.h>
#include <fcntl.h>
#include <internal/__syscalls.h>

int write(long fd, void* buf, long count) {
  int ret = __syscall3(__HOJICHA_SYS_SYSCALL_WRITE, fd, (long)buf, count);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}

