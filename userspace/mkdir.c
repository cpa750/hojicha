#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char** argv) {
  if (argc <= 1) {
    printf("mkdir: missing operand\n");
    return 1;
  }

  int ret = 0;
  for (int i = 1; i < argc; ++i) {
    if (mkdir(argv[i]) < 0) {
      printf("mkdir: cannot create %s: %d\n", argv[i], errno);
      ret = 1;
    }
  }

  return ret;
}
