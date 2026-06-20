#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
  if (argc <= 1) {
    printf("rm: missing operand\n");
    return 1;
  }

  int ret = 0;
  for (int i = 1; i < argc; ++i) {
    if (unlink(argv[i]) < 0) {
      printf("rm: cannot remove %s: %d\n", argv[i], errno);
      ret = 1;
    }
  }

  return ret;
}
