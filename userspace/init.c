#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
  printf("hello from init\n");

  int pid = fork();
  if (pid < 0) {
    printf("fork failed: %d\n", (unsigned long)errno);
    return 1;
  }

  if (pid == 0) {
    printf("launching ls...");
    char* argv[] = {"ls", 0};
    execve("/usr/bin/ls", argv, 0);
    printf("execve failed: %d\n", (unsigned long)errno);
    return 1;
  }

  printf("init launched ls as pid %d\n", (unsigned long)pid);
  return 0;
}
