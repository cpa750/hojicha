#include <errno.h>
#include <sys/__syscalls.h>
#include <sys/stat.h>

int mkdir(const char* path) {
  int ret = __syscall1(__HOJICHA_SYS_SYSCALL_MKDIR, (long)path);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
