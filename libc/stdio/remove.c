#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int remove(const char* path) {
  if (unlink(path) == 0) { return 0; }
  if (errno == EISDIR) { return rmdir(path); }
  return -1;
}
