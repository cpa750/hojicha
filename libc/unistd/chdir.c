#include <errno.h>
#include <internal/__syscalls.h>
#include <unistd.h>

int chdir(const char* path) {
  int ret = __syscall1(__HOJICHA_INTERNAL_SYSCALL_CHDIR, (long)path);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}

int fchdir(int fd) {
  int ret = __syscall1(__HOJICHA_INTERNAL_SYSCALL_FCHDIR, fd);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
