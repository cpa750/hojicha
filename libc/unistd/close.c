#include <errno.h>
#include <internal/__syscalls.h>
#include <unistd.h>

int close(int fd) {
  int ret = __syscall1(__HOJICHA_SYS_SYSCALL_CLOSE, fd);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
