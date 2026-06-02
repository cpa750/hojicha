#include <errno.h>
#include <sys/__syscalls.h>
#include <unistd.h>

int unlink(const char* path) {
  int ret = __syscall1(__HOJICHA_SYS_SYSCALL_UNLINK, (long)path);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
