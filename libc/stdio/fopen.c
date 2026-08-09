#include <errno.h>
#include <fcntl.h>
#include <internal/__stdio.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int fopen_flags_from_mode(const char* mode, int* flags_out) {
  if (mode == NULL || flags_out == NULL) { return -1; }

  int flags;
  switch (mode[0]) {
    case 'r':
      flags = O_RDONLY;
      break;
    case 'w':
      flags = O_WRONLY | O_CREAT | O_TRUNC;
      break;
    case 'a':
      flags = O_WRONLY | O_CREAT | O_APPEND;
      break;
    default:
      return -1;
  }

  for (const char* c = mode + 1; *c != '\0'; ++c) {
    if (*c == 'b') { continue; }
    if (*c == '+') {
      flags = (flags & ~O_ACCMODE) | O_RDWR;
      continue;
    }
    return -1;
  }

  *flags_out = flags;
  return 0;
}

FILE* fopen(const char* restrict path, const char* restrict mode) {
  if (path == NULL) {
    errno = EINVAL;
    return NULL;
  }

#if defined(__is_libk)
  (void)mode;
  errno = ENOSYS;
  return NULL;
#else
  int flags;
  if (fopen_flags_from_mode(mode, &flags) < 0) {
    errno = EINVAL;
    return NULL;
  }

  int fd = open(path, flags, 0666);
  if (fd < 0) { return NULL; }

  FILE* stream = malloc(sizeof(FILE));
  if (stream == NULL) {
    close(fd);
    errno = ENOMEM;
    return NULL;
  }

  stream->fd = fd;
  stream->flags = __HOJICHA_FILE_OWNED;
  return stream;
#endif
}
