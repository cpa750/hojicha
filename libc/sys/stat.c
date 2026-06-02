#include <errno.h>
#include <sys/__syscalls.h>
#include <sys/stat.h>

int stat(const char* path, stat_t* stat_buf) {
  int ret =
      __syscall2(__HOJICHA_SYS_SYSCALL_STAT, (long)path, (long)stat_buf);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
