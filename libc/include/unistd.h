#ifndef HOJICHA_UNISTD_H
#define HOJICHA_UNISTD_H

#include <stdint.h>
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#ifdef __cplusplus
extern "C" {
#endif

int access(const char* path, int amode);
int close(int fd);
int execve(const char* pathname, char* const argv[], char* const envp[]);
int fork(void);
long lseek(int fd, long offset, int whence);
int rmdir(const char* path);
unsigned int sleep(unsigned int seconds);
int unlink(const char* path);

int brk(void* addr);
void* sbrk(intptr_t offset);

#ifdef __cplusplus
}
#endif

#endif  // HOJICHA_UNISTD_H
