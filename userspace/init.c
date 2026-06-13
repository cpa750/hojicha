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

static int launch_bigmaths(void) {
  int pid = fork();
  if (pid < 0) {
    printf("fork failed: %d\n", (unsigned long)errno);
    return -1;
  }

  if (pid == 0) {
    printf("launching bigmaths...\n");
    char* argv[] = {"bigmaths", "from-init", 0};
    char* envp[] = {"PATH=/usr/bin", "HOJICHA=tea", 0};
    execve("/usr/bin/bigmaths", argv, envp);
    printf("execve failed: %d\n", (unsigned long)errno);
    return 1;
  }

  printf("init launched bigmaths as pid %d\n", (unsigned long)pid);
  return 0;
}

int main(void) {
  printf("hello from init\n");

  if (launch("ls", "/usr/bin/ls") < 0) { return 1; }
  if (launch_bigmaths() < 0) { return 1; }
  if (launch("hmalloc_test", "/usr/bin/hmalloc_test") < 0) { return 1; }

  return 0;
}
