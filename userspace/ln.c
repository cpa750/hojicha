#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void usage(void) { printf("ln: usage: ln [-s] target linkpath\n"); }

int main(int argc, char** argv) {
  int status = 0;

  if (argc == 3) {
    status = link(argv[1], argv[2]);
  } else if (argc == 4 && strcmp(argv[1], "-s") == 0) {
    status = symlink(argv[2], argv[3]);
  } else {
    usage();
    return 1;
  }

  if (status < 0) {
    printf("ln: cannot link %s to %s: %d\n",
           argc == 3 ? argv[1] : argv[2],
           argc == 3 ? argv[2] : argv[3],
           errno);
    return 1;
  }

  return 0;
}
