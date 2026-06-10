#include <errno.h>
#include <sys/__syscalls.h>
#include <unistd.h>

int execve(const char* pathname, char* const argv[], char* const envp[]) {
  int ret = __syscall3(__HOJICHA_SYS_SYSCALL_EXECVE,
                       (long)pathname,
                       (long)argv,
                       (long)envp);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
