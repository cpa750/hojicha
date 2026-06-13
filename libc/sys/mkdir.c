#include <errno.h>
#include <internal/__syscalls.h>
#include <sys/stat.h>

int mkdir(const char* path) {
  int ret = __syscall1(__HOJICHA_INTERNAL_SYSCALL_MKDIR, (long)path);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
