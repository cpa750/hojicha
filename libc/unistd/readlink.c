#include <errno.h>
#include <internal/__syscalls.h>
#include <unistd.h>

long readlink(const char* path, char* buf, long bufsiz) {
  long ret = __syscall3(
      __HOJICHA_INTERNAL_SYSCALL_READLINK, (long)path, (long)buf, bufsiz);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
