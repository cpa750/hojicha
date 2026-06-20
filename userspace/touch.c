#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int touch_path(const char* path) {
  int fd = open(path, O_RDWR | O_CREAT, 0);
  if (fd < 0) {
    printf("touch: cannot touch %s: %d\n", path, errno);
    return 1;
  }

  char byte = 0;
  if (read(fd, &byte, 0) < 0) {
    printf("touch: cannot update atime for %s: %d\n", path, errno);
    close(fd);
    return 1;
  }
  if (write(fd, &byte, 0) < 0) {
    printf("touch: cannot update mtime for %s: %d\n", path, errno);
    close(fd);
    return 1;
  }

  close(fd);
  return 0;
}

int main(int argc, char** argv) {
  if (argc <= 1) {
    printf("touch: missing file operand\n");
    return 1;
  }

  int ret = 0;
  for (int i = 1; i < argc; ++i) {
    if (touch_path(argv[i]) != 0) { ret = 1; }
  }

  return ret;
}
