#include <stdio.h>
#include <string.h>

int main(int argc, char** argv, char** envp) {
  char line[4096];
  while (1) {
    printf("> ");

    if (fgets(line, sizeof(line), stdin) == 0) {
      printf("\n");
      break;
    }

    size_t line_len = strlen(line);
    if (line_len > 0 && line[line_len - 1] == '\n') { line[--line_len] = '\0'; }

    if (line_len == 0) { continue; }

    if (strcmp(line, "exit") == 0) { break; }

    printf("sh: command not found: %s\n", line);
  }

  return 0;
}
