#include <errno.h>
#include <internal/__syscalls.h>
#include <unistd.h>

int execve(const char* pathname, char* const argv[], char* const envp[]) {
  int ret = __syscall3(__HOJICHA_INTERNAL_SYSCALL_EXECVE,
                       (long)pathname,
                       (long)argv,
                       (long)envp);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
