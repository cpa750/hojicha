#ifndef HOJICHA_SYS_IOCTL_H
#define HOJICHA_SYS_IOCTL_H

#ifdef __cplusplus
extern "C" {
#endif

int ioctl(int fd, unsigned long request, void* arg);

#ifdef __cplusplus
}
#endif

#endif  // HOJICHA_SYS_IOCTL_H
