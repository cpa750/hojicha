#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define DIRENT_BUF_SIZE 512
#define PATH_BUF_SIZE   256

static const char* paths_to_list[] = {
    "/",
    "/etc",
    "/usr/bin",
    "/dev",
};

static char mode_type_char(uint32_t mode) {
  switch (mode & S_IFMT) {
    case S_IFDIR:
      return 'd';
    case S_IFREG:
      return 'f';
    case S_IFCHR:
      return 'c';
    case S_IFLNK:
      return 'l';
    default:
      return '?';
  }
}

static int build_child_path(const char* dir, const char* name, char* out) {
  uint64_t dir_len = strlen(dir);
  uint64_t name_len = strlen(name);
  uint64_t needs_slash = dir_len > 1 && dir[dir_len - 1] != '/';
  uint64_t total_len = dir_len + needs_slash + name_len;

  if (total_len + 1 > PATH_BUF_SIZE) { return -1; }

  memcpy(out, dir, dir_len);
  if (needs_slash) { out[dir_len++] = '/'; }
  memcpy(out + dir_len, name, name_len);
  out[total_len] = '\0';
  return 0;
}

static void print_entry(const char* dir, linux_dirent_t* dirent) {
  char path[PATH_BUF_SIZE];
  stat_t st;

  if (build_child_path(dir, dirent->d_name, path) < 0) {
    printf("? 0 %s\n", dirent->d_name);
    return;
  }

  if (stat(path, &st) < 0) {
    printf("? 0 %s\n", dirent->d_name);
    return;
  }

  printf("%c %d %s\n", mode_type_char(st.st_mode), st.st_size, dirent->d_name);
}

static void list_dir(const char* path) {
  char dirent_buf[DIRENT_BUF_SIZE];

  printf("%s:\n", path);

  int fd = open(path, O_RDONLY | O_DIRECTORY, 0);
  if (fd < 0) {
    printf("open failed: %d\n\n", (uint64_t)errno);
    return;
  }

  while (1) {
    int bytes = getdents(fd, (linux_dirent_t*)dirent_buf, DIRENT_BUF_SIZE);
    if (bytes < 0) {
      printf("getdents failed: %d\n", (uint64_t)errno);
      break;
    }
    if (bytes == 0) { break; }

    int offset = 0;
    while (offset < bytes) {
      linux_dirent_t* dirent = (linux_dirent_t*)(dirent_buf + offset);
      print_entry(path, dirent);
      offset += dirent->d_reclen;
    }
  }

  printf("\n");
}

int main(void) {
  uint64_t count = sizeof(paths_to_list) / sizeof(paths_to_list[0]);
  for (uint64_t i = 0; i < count; ++i) { list_dir(paths_to_list[i]); }
  return 0;
}
