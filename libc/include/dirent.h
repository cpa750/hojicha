#ifndef HOJICHA_DIRENT_H
#define HOJICHA_DIRENT_H

typedef struct linux_dirent linux_dirent_t;

// Source: https://www.man7.org/linux/man-pages/man2/getdents.2.html
struct linux_dirent {
  unsigned long d_ino;
  unsigned long d_off;
  unsigned short d_reclen;
  char d_name[];
};

#ifdef __cplusplus
extern "C" {
#endif

int getdents(unsigned int fd, linux_dirent_t* dirent_buf, unsigned int count);

#ifdef __cplusplus
}
#endif

#endif  // HOJICHA_DIRENT_H
