#include <errno.h>
#include <internal/__syscalls.h>
#include <unistd.h>

int unlink(const char* path) {
  int ret = __syscall1(__HOJICHA_INTERNAL_SYSCALL_UNLINK, (long)path);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
