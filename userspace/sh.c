#include <stdio.h>

int main(int argc, char** argv, char** envp) {
  for (int i = 0; i < argc; ++i) { printf("argv[%d]: %s\n", i, argv[i]); }

  if (envp != 0) {
    for (int i = 0; envp[i] != 0; ++i) { printf("envp[%d]: %s\n", i, envp[i]); }
  }

  printf("> ");

  return 0;
}
