#ifndef HOJICHA_UNISTD_H
#define HOJICHA_UNISTD_H

#include <stdint.h>
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#ifdef __cplusplus
extern "C" {
#endif

int access(const char* path, int amode);
int chdir(const char* path);
int close(int fd);
int dup2(int oldfd, int newfd);
int execve(const char* pathname, char* const argv[], char* const envp[]);
int fchdir(int fd);
int fork(void);
char* getcwd(char* buf, unsigned long size);
int link(const char* oldpath, const char* newpath);
long lseek(int fd, long offset, int whence);
long readlink(const char* path, char* buf, long bufsiz);
int rmdir(const char* path);
unsigned int sleep(unsigned int seconds);
int symlink(const char* target, const char* linkpath);
int unlink(const char* path);
int usleep(unsigned long usec);

int brk(void* addr);
void* sbrk(intptr_t offset);

#ifdef __cplusplus
}
#endif

#endif  // HOJICHA_UNISTD_H
