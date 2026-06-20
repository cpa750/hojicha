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

int link(const char* oldpath, const char* newpath) {
  int ret = __syscall2(
      __HOJICHA_INTERNAL_SYSCALL_LINK, (long)oldpath, (long)newpath);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}

int symlink(const char* target, const char* linkpath) {
  int ret = __syscall2(
      __HOJICHA_INTERNAL_SYSCALL_SYMLINK, (long)target, (long)linkpath);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}

long readlink(const char* path, char* buf, long bufsiz) {
  long ret = __syscall3(
      __HOJICHA_INTERNAL_SYSCALL_READLINK, (long)path, (long)buf, bufsiz);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
