#include <errno.h>
#include <sys/__syscalls.h>
#include <unistd.h>

long lseek(int fd, long offset, int whence) {
  long ret = __syscall3(__HOJICHA_SYS_SYSCALL_LSEEK, fd, offset, whence);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
