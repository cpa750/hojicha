#include <errno.h>
#include <internal/__syscalls.h>
#include <unistd.h>

int link(const char* oldpath, const char* newpath) {
  int ret = __syscall2(
      __HOJICHA_INTERNAL_SYSCALL_LINK, (long)oldpath, (long)newpath);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
