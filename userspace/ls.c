#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DIRENT_BUF_SIZE 512

static const char* basename(const char* path) {
  const char* name = path;

  for (const char* slash = strchr(path, '/'); slash != NULL;
       slash = strchr(slash + 1, '/')) {
    if (slash[1] != '\0') { name = slash + 1; }
  }

  return name;
}

static void list_file(const char* path) {
  printf("%s\n", basename(path));
}

static void list_dir(const char* path) {
  char dirent_buf[DIRENT_BUF_SIZE];

  int fd = open(path, O_RDONLY | O_DIRECTORY, 0);
  if (fd < 0) {
    printf("ls: cannot open %s: %d\n", path, errno);
    return;
  }

  while (1) {
    int bytes = getdents(fd, (linux_dirent_t*)dirent_buf, sizeof(dirent_buf));
    if (bytes < 0) {
      printf("ls: cannot read %s: %d\n", path, errno);
      break;
    }
    if (bytes == 0) { break; }

    int offset = 0;
    while (offset < bytes) {
      linux_dirent_t* dirent = (linux_dirent_t*)(dirent_buf + offset);
      printf("%s\n", dirent->d_name);
      offset += dirent->d_reclen;
    }
  }

  close(fd);
}

static void list_path(const char* path, int print_header) {
  if (path[0] != '/') {
    printf("ls: relative paths are not supported: %s\n", path);
    return;
  }

  stat_t st;
  if (stat(path, &st) < 0) {
    printf("ls: missing: %s\n", path);
    return;
  }

  if ((st.st_mode & S_IFMT) == S_IFDIR) {
    if (print_header) { printf("%s:\n", path); }
    list_dir(path);
    return;
  }

  list_file(path);
}

int main(int argc, char** argv) {
  if (argc <= 1) {
    list_path("/", 0);
    return 0;
  }

  for (int i = 1; i < argc; ++i) {
    if (i > 1) { printf("\n"); }
    list_path(argv[i], argc > 2);
  }

  return 0;
}
