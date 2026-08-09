#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int rename(const char* oldpath, const char* newpath) {
  if (link(oldpath, newpath) < 0) { return -1; }

  if (unlink(oldpath) < 0) {
    int saved_errno = errno;
    unlink(newpath);
    errno = saved_errno;
    return -1;
  }

  return 0;
}
