#include <errno.h>
#include <internal/__syscalls.h>
#include <sys/mman.h>

void* mmap(void* addr,
           size_t length,
           int prot,
           int flags,
           int fd,
           long offset) {
  long ret = __syscall6(__HOJICHA_INTERNAL_SYSCALL_MMAP,
                        (long)addr,
                        (long)length,
                        prot,
                        flags,
                        fd,
                        offset);
  if (ret < 0) {
    errno = -ret;
    return MAP_FAILED;
  }
  return (void*)ret;
}

int munmap(void* addr, size_t length) {
  long ret =
      __syscall2(__HOJICHA_INTERNAL_SYSCALL_MUNMAP, (long)addr, length);
  if (ret < 0) {
    errno = -ret;
    return -1;
  }
  return 0;
}
