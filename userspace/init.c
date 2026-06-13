#include <errno.h>
#include <stdio.h>
#include <unistd.h>

static int launch_shell(void) {
  int pid = fork();
  if (pid < 0) {
    printf("fork failed: %d\n", (unsigned long)errno);
    return -1;
  }

  if (pid == 0) {
    char* argv[] = {"sh", 0};
    char* envp[] = {"PATH=/usr/bin", 0};
    execve("/usr/bin/sh", argv, envp);
    printf("execve failed: %d\n", (unsigned long)errno);
    return 1;
  }

  return 0;
}

int main(void) {
  printf("Welcome to Hojicha. Pour yourself a cup and let's get started!\n");

  if (launch_shell() < 0) { return 1; }

  // TODO: waitpid here and relaunch sh if needed

  return 0;
}
