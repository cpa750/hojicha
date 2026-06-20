#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "sys/wait.h"

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

  return pid;
}

int main(void) {
  printf(""
         "    ))  ))\n"
         "   ((  ((\n"
         "    ))  ))\n"
         "  .((..((...\n"
         ".'  ))  ))  '.\n"
         "|'..........'|\n"
         "|            |\n"
         "|            |\n"
         "|            |\n"
         " '.        .'\n"
         "   '------'    Welcome to Hojicha. Pour yourself a cup and let's "
         "get started!\n\n");

  for (;;) {
    int shell_pid = launch_shell();
    if (shell_pid < 0) { return 1; }
    waitpid(shell_pid, NULL, 0);
  }

  return 0;
}
