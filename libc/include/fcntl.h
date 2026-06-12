#ifndef HOJICHA_FCNTL_H
#define HOJICHA_FCNTL_H

#include <sys/cdefs.h>

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_ACCMODE 3

#define O_CREAT     0100
#define O_CLOEXEC   02000000
#define O_DIRECTORY 0200000

#ifdef __cplusplus
extern "C" {
#endif

int open(const char* path, int flags, int mode);
int read(long fd, void* buf, long count);
int write(long fd, void* buf, long count);

#ifdef __cplusplus
}
#endif

#endif  // HOJICHA_FCNTL_H
