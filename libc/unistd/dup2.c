#include <errno.h>
#include <internal/__syscalls.h>
#include <unistd.h>

int dup2(int oldfd, int newfd) {
  int ret = __syscall2(__HOJICHA_INTERNAL_SYSCALL_DUP2, oldfd, newfd);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
