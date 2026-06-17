#ifndef HOJICHA_ERRNO_H
#define HOJICHA_ERRNO_H

// Should be more sophisticated but for now this kludge will do
#define errno __errno

extern int __errno;

#define EBADF     9
#define ECHILD    10
#define EAGAIN    11
#define ENOMEM    12
#define EACCES    13
#define EEXIST    17
#define ENOENT    2
#define ENOTDIR   20
#define EISDIR    21
#define EINVAL    22
#define EMFILE    24
#define ENOSYS    38
#define ENOTEMPTY 39

#endif  // HOJICHA_ERRNO_H
