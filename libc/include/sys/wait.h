#ifndef HOJICHA_SYS_WAIT_H
#define HOJICHA_SYS_WAIT_H

#include <sys/cdefs.h>

#define WNOHANG 1

#define WEXITSTATUS(status) (((status) >> 8) & 0xFF)
#define WIFEXITED(status)   (1)

#ifdef __cplusplus
extern "C" {
#endif

int waitpid(int pid, int* wstatus, int options);

#ifdef __cplusplus
}
#endif

#endif  // HOJICHA_SYS_WAIT_H
