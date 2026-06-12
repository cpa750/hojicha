#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv, char** envp) {
  printf("sleep(2);\n");
  sleep(2);
  printf("argc: %d\n", argc);
  for (int i = 0; i < argc; ++i) { printf("argv[%d]: %s\n", i, argv[i]); }

  if (envp != 0) {
    for (int i = 0; envp[i] != 0; ++i) { printf("envp[%d]: %s\n", i, envp[i]); }
  }

  printf("2 + 2 is 4, - 1 that's 3, big maths!\n");
  return 0;  // Big maths
}
