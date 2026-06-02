#ifndef HOJICHA_FS_VFS_UTILS_H
#define HOJICHA_FS_VFS_UTILS_H

#include <stddef.h>

#define SET_OUT(out, val)                                                      \
  do {                                                                         \
    if ((out) != NULL) { *(out) = (val); }                                     \
  } while (0)
#define SET_OUT_NULL(out) SET_OUT(out, NULL)

#endif
