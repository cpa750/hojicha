#ifndef HOJICHA_FCNTL_H
#define HOJICHA_FCNTL_H

#include <sys/cdefs.h>

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
