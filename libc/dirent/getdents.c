#include <dirent.h>
#include <errno.h>
#include <internal/__syscalls.h>

int getdents(unsigned int fd, linux_dirent_t* dirent_buf, unsigned int count) {
  int ret = __syscall3(__HOJICHA_INTERNAL_SYSCALL_GETDENTS,
                       fd,
                       (long)dirent_buf,
                       count);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return ret;
}
