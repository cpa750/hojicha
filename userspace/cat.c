#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define CAT_BUF_SIZE 512
#define STDOUT_FD    1

static void cat_file(const char* path) {
  if (path[0] != '/') {
    printf("cat: relative paths are not supported: %s\n", path);
    return;
  }

  int fd = open(path, O_RDONLY, 0);
  if (fd < 0) {
    printf("cat: cannot open %s: %d\n", path, errno);
    return;
  }

  char buf[CAT_BUF_SIZE];
  while (1) {
    int bytes_read = read(fd, buf, sizeof(buf));
    if (bytes_read < 0) {
      printf("cat: cannot read %s: %d\n", path, errno);
      break;
    }
    if (bytes_read == 0) { break; }

    int bytes_written = write(STDOUT_FD, buf, bytes_read);
    if (bytes_written < 0) {
      printf("cat: cannot write stdout: %d\n", errno);
      break;
    }
  }

  printf("\n");

  close(fd);
}

int main(int argc, char** argv) {
  if (argc <= 1) {
    printf("cat: missing file operand\n");
    return 1;
  }

  for (int i = 1; i < argc; ++i) { cat_file(argv[i]); }

  return 0;
}
