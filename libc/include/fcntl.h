#ifndef HOJICHA_FCNTL_H
#define HOJICHA_FCNTL_H

#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

int open(const char* path, int flags, int mode);

#ifdef __cplusplus
}
#endif

#endif  // HOJICHA_FCNTL_H
