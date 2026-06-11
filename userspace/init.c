#include <errno.h>
#include <stdio.h>
#include <unistd.h>

static int launch(const char* name, const char* path) {
  int pid = fork();
  if (pid < 0) {
    printf("fork failed: %d\n", (unsigned long)errno);
    return -1;
  }

  if (pid == 0) {
    printf("launching %s...\n", name);
    char* argv[] = {(char*)name, 0};
    execve(path, argv, 0);
    printf("execve failed: %d\n", (unsigned long)errno);
    return 1;
  }

  printf("init launched %s as pid %d\n", name, (unsigned long)pid);
  return 0;
}

int main(void) {
  printf("hello from init\n");

  if (launch("ls", "/usr/bin/ls") < 0) { return 1; }
  if (launch("bigmaths", "/usr/bin/bigmaths") < 0) { return 1; }

  return 0;
}
