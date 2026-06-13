#include <errno.h>
#include <internal/__syscalls.h>
#include <sys/ioctl.h>

int ioctl(int fd, unsigned long request, void* arg) {
  int ret = __syscall3(__HOJICHA_INTERNAL_SYSCALL_IOCTL, fd, request, (long)arg);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
