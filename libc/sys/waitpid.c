#include <errno.h>
#include <internal/__syscalls.h>
#include <sys/wait.h>

int waitpid(int pid, int* wstatus, int options) {
  int ret = __syscall3(__HOJICHA_INTERNAL_SYSCALL_WAITPID,
                       pid,
                       (long)wstatus,
                       options);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
