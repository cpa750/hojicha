#ifndef HOJICHA_STAT_H
#define HOJICHA_STAT_H

#include <stdint.h>

// Adapted from https://www.man7.org/linux/man-pages/man3/stat.3type.html and
// https://www.man7.org/linux/man-pages/man3/timespec.3type.html

typedef struct timespec timespec_t;
struct timespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};

typedef struct stat stat_t;
struct stat {
  uint64_t st_dev;
  uint64_t st_ino;

  uint32_t st_mode;
  uint32_t st_nlink;
  uint32_t st_uid;
  uint32_t st_gid;

  uint64_t st_rdev;
  uint64_t st_size;

  uint64_t st_blksize;
  uint64_t st_blocks;

  timespec_t st_atim;
  timespec_t st_mtim;
  timespec_t st_ctim;

#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
};

#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000

#ifdef __cplusplus
extern "C" {
#endif

int fstat(int fd, stat_t* stat_buf);
int mkdir(const char* path);
int stat(const char* path, stat_t* stat_buf);

#ifdef __cplusplus
}
#endif

#endif  // HOJICHA_STAT_H
