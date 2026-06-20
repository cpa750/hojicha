#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define SH_ARG_MAX   64
#define SH_PATH_MAX  512
#define SH_REDIR_MAX 8

typedef enum {
  SH_REDIR_FILE,
  SH_REDIR_DUP,
} sh_redir_kind_t;

typedef struct {
  sh_redir_kind_t kind;
  int fd;
  int target_fd;
  int flags;
  char* path;
} sh_redir_t;

static void free_command(char** argv,
                         int argc,
                         sh_redir_t* redirs,
                         int redir_count) {
  for (int i = 0; i < argc; ++i) { free(argv[i]); }
  for (int i = 0; i < redir_count; ++i) {
    if (redirs[i].kind == SH_REDIR_FILE) { free(redirs[i].path); }
  }
}

static int parse_word(char** cursor, char** out) {
  char* p = *cursor;
  if (*p == '\0' || isspace(*p) || *p == '<' || *p == '>') { return -1; }

  char* start = p;
  while (*p != '\0' && !isspace(*p) && *p != '<' && *p != '>') { ++p; }

  size_t len = p - start;
  char* word = malloc(len + 1);
  if (word == NULL) {
    printf("sh: out of memory\n");
    return -1;
  }
  memcpy(word, start, len);
  word[len] = '\0';

  *out = word;
  *cursor = p;
  return 0;
}

static int parse_redirection(char** cursor,
                             sh_redir_t* redirs,
                             int* redir_count,
                             int max_redirs) {
  char* p = *cursor;
  int fd = 1;

  if (*p == '<') {
    fd = 0;
    ++p;
  } else if (*p == '>') {
    ++p;
  } else if (p[0] == '2' && p[1] == '>') {
    fd = 2;
    p += 2;
  } else {
    return -1;
  }

  if (*redir_count == max_redirs) {
    printf("sh: too many redirections\n");
    return -1;
  }

  int append = 0;
  if (fd != 0 && *p == '>') {
    append = 1;
    ++p;
  }

  while (isspace(*p)) { ++p; }

  char* target = NULL;
  if (parse_word(&p, &target) != 0) {
    printf("sh: missing redirection target\n");
    return -1;
  }

  sh_redir_t* redir = &redirs[(*redir_count)++];
  redir->fd = fd;
  redir->target_fd = -1;
  redir->path = NULL;

  if (target[0] == '&' && target[1] >= '0' && target[1] <= '9' &&
      target[2] == '\0') {
    redir->kind = SH_REDIR_DUP;
    redir->target_fd = target[1] - '0';
    free(target);
  } else {
    redir->kind = SH_REDIR_FILE;
    redir->path = target;
    redir->flags =
        fd == 0 ? O_RDONLY : O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
  }

  *cursor = p;
  return 0;
}

static int parse_command(char* line,
                         char** argv,
                         int max_args,
                         sh_redir_t* redirs,
                         int max_redirs,
                         int* redir_count) {
  int argc = 0;
  char* p = line;
  *redir_count = 0;

  while (*p != '\0') {
    while (isspace(*p)) { ++p; }
    if (*p == '\0') { break; }

    if (*p == '<' || *p == '>' || (p[0] == '2' && p[1] == '>')) {
      if (parse_redirection(&p, redirs, redir_count, max_redirs) != 0) {
        free_command(argv, argc, redirs, *redir_count);
        *redir_count = 0;
        return -1;
      }
      continue;
    }

    if (argc == max_args - 1) {
      printf("sh: too many arguments\n");
      free_command(argv, argc, redirs, *redir_count);
      *redir_count = 0;
      return -1;
    }

    char* arg = NULL;
    if (parse_word(&p, &arg) != 0) {
      free_command(argv, argc, redirs, *redir_count);
      *redir_count = 0;
      return -1;
    }
    argv[argc++] = arg;
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

static int apply_redirections(sh_redir_t* redirs, int redir_count) {
  for (int i = 0; i < redir_count; ++i) {
    sh_redir_t* redir = &redirs[i];
    if (redir->kind == SH_REDIR_DUP) {
      if (dup2(redir->target_fd, redir->fd) < 0) {
        printf("sh: dup2 failed: %d\n", errno);
        return -1;
      }
      continue;
    }

    int fd = open(redir->path, redir->flags, 0666);
    if (fd < 0) {
      printf("sh: cannot open %s: %d\n", redir->path, errno);
      return -1;
    }

    if (fd != redir->fd) {
      if (dup2(fd, redir->fd) < 0) {
        printf("sh: dup2 failed: %d\n", errno);
        close(fd);
        return -1;
      }
      close(fd);
    }
  }

  return 0;
}

static void launch(const char* path,
                   char** argv,
                   char** envp,
                   sh_redir_t* redirs,
                   int redir_count) {
  int pid = fork();
  if (pid < 0) {
    printf("sh: fork failed: %d\n", errno);
    return;
  }

  if (pid == 0) {
    if (apply_redirections(redirs, redir_count) != 0) { exit(126); }
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
    sh_redir_t redirs[SH_REDIR_MAX];
    int redir_count = 0;
    int command_argc = parse_command(
        p, command_argv, SH_ARG_MAX, redirs, SH_REDIR_MAX, &redir_count);
    if (command_argc <= 0) {
      if (command_argc == 0) {
        free_command(command_argv, command_argc, redirs, redir_count);
      }
      continue;
    }

    if (strcmp(command_argv[0], "exit") == 0) {
      free_command(command_argv, command_argc, redirs, redir_count);
      break;
    }
    if (strcmp(command_argv[0], "cd") == 0) {
      const char* path = command_argc > 1 ? command_argv[1] : getenv("HOME");
      if (command_argc > 2) {
        printf("cd: too many arguments\n");
      } else if (chdir(path == NULL ? "/" : path) < 0) {
        printf("cd: cannot cd to %s: %d\n", path == NULL ? "/" : path, errno);
      }
      free_command(command_argv, command_argc, redirs, redir_count);
      continue;
    }

    char path_buf[SH_PATH_MAX];
    char* executable =
        find_executable(command_argv[0], path_buf, sizeof(path_buf));
    if (executable == NULL) {
      printf("sh: command not found: %s\n", command_argv[0]);
      free_command(command_argv, command_argc, redirs, redir_count);
      continue;
    }

    launch(executable, command_argv, envp, redirs, redir_count);
    free_command(command_argv, command_argc, redirs, redir_count);
  }

  return 0;
}
