#include <errno.h>
#include <internal/__syscalls.h>
#include <unistd.h>

int symlink(const char* target, const char* linkpath) {
  int ret = __syscall2(
      __HOJICHA_INTERNAL_SYSCALL_SYMLINK, (long)target, (long)linkpath);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
