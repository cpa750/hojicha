#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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

static void print_header(void) {
  printf("Name\tTime accessed\tTime modified\tTime changed\n");
}

static void print_entry(const char* name, const stat_t* st) {
  printf("%s\t%d\t%d\t%d\n",
         name,
         (uint64_t)st->st_atime,
         (uint64_t)st->st_mtime,
         (uint64_t)st->st_ctime);
}

static char* join_path(const char* dir, const char* name) {
  size_t dir_len = strlen(dir);
  size_t name_len = strlen(name);
  int needs_slash = dir_len > 0 && dir[dir_len - 1] != '/';
  char* path = malloc(dir_len + needs_slash + name_len + 1);
  if (path == NULL) { return NULL; }

  memcpy(path, dir, dir_len);
  size_t offset = dir_len;
  if (needs_slash) { path[offset++] = '/'; }
  memcpy(path + offset, name, name_len);
  path[offset + name_len] = '\0';
  return path;
}

static void list_file(const char* path, const stat_t* st) {
  print_entry(basename(path), st);
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
      char* entry_path = join_path(path, dirent->d_name);
      stat_t entry_st;
      if (entry_path != NULL && stat(entry_path, &entry_st) == 0) {
        print_entry(dirent->d_name, &entry_st);
      } else {
        printf("%s\t?\t?\t?\n", dirent->d_name);
      }
      free(entry_path);
      offset += dirent->d_reclen;
    }
  }

  close(fd);
}

static void list_path(const char* path, int include_path_header) {
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
    if (include_path_header) { printf("%s:\n", path); }
    print_header();
    list_dir(path);
    return;
  }

  print_header();
  list_file(path, &st);
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
