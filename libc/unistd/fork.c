#include <errno.h>
#include <sys/__syscalls.h>
#include <unistd.h>

int fork(void) {
  int ret = __syscall0(__HOJICHA_SYS_SYSCALL_FORK);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
