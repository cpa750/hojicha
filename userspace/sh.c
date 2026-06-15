#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define SH_ARG_MAX  64
#define SH_PATH_MAX 512

static int parse_args(char* line, char** argv, int max_args) {
  int argc = 0;
  char* saveptr = NULL;
  char* arg = strtok_r(line, " \t\r\n", &saveptr);

  while (arg != NULL) {
    if (argc == max_args - 1) {
      printf("sh: too many arguments\n");
      return -1;
    }

    argv[argc++] = arg;
    arg = strtok_r(NULL, " \t\r\n", &saveptr);
  }

  argv[argc] = NULL;
  return argc;
}

static char* find_executable(char* command,
                             char* path_buf,
                             size_t path_buf_size) {
  if (strchr(command, '/') != NULL) {
    return access(command, X_OK) == 0 ? command : NULL;
  }

  char* path = getenv("PATH");
  if (path == NULL) { return NULL; }

  char* path_copy = strdup(path);
  if (path_copy == NULL) { return NULL; }

  char* saveptr = NULL;
  char* dir = strtok_r(path_copy, ":", &saveptr);
  while (dir != NULL) {
    const char* sep = dir[strlen(dir) - 1] == '/' ? "" : "/";
    int len = snprintf(path_buf, path_buf_size, "%s%s%s", dir, sep, command);
    if (len >= 0 && (size_t)len < path_buf_size &&
        access(path_buf, X_OK) == 0) {
      free(path_copy);
      return path_buf;
    }

    dir = strtok_r(NULL, ":", &saveptr);
  }

  free(path_copy);
  return NULL;
}

static void launch(const char* path, char** argv, char** envp) {
  int pid = fork();
  if (pid < 0) {
    printf("sh: fork failed: %d\n", errno);
    return;
  }

  if (pid == 0) {
    execve(path, argv, envp);
    printf("sh: execve failed: %s: %d\n", path, errno);
    exit(127);
  }

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    printf("sh: waitpid failed: %d\n", errno);
  }
}

int main(int argc, char** argv, char** envp) {
  char line[4096];
  while (1) {
    printf("> ");

    if (fgets(line, sizeof(line), stdin) == NULL) {
      printf("\n");
      break;
    }

    char* p = line;
    while (isspace(*p)) { ++p; }
    if (*p == '\0') { continue; }

    char* command_argv[SH_ARG_MAX];
    int command_argc = parse_args(p, command_argv, SH_ARG_MAX);
    if (command_argc <= 0) { continue; }

    if (strcmp(command_argv[0], "exit") == 0) { break; }

    char path_buf[SH_PATH_MAX];
    char* executable =
        find_executable(command_argv[0], path_buf, sizeof(path_buf));
    if (executable == NULL) {
      printf("sh: command not found: %s\n", command_argv[0]);
      continue;
    }

    launch(executable, command_argv, envp);
  }

  return 0;
}
