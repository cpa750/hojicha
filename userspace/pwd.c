#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
  char cwd[4096];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    printf("pwd: getcwd failed: %d\n", errno);
    return 1;
  }

  printf("%s\n", cwd);
  return 0;
}
