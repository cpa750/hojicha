#ifndef HOJICHA_UNISTD_H
#define HOJICHA_UNISTD_H

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#ifdef __cplusplus
extern "C" {
#endif

int close(int fd);
long lseek(int fd, long offset, int whence);
int rmdir(const char* path);
unsigned int sleep(unsigned int seconds);
int unlink(const char* path);

#ifdef __cplusplus
}
#endif

#endif  // HOJICHA_UNISTD_H
