#include <errno.h>
#include <internal/__syscalls.h>
#include <stddef.h>
#include <unistd.h>

char* getcwd(char* buf, unsigned long size) {
  long ret = __syscall2(__HOJICHA_INTERNAL_SYSCALL_GETCWD, (long)buf, size);
  if (ret < 0) {
    errno = -ret;
    return NULL;
  }
  return (char*)ret;
}
