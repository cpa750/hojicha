#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int vfprintf(FILE* restrict stream,
             const char* restrict format,
             va_list parameters) {
  if (stream == NULL || format == NULL) {
    errno = EINVAL;
    return -1;
  }

  va_list copy;
  va_copy(copy, parameters);
  int len = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if (len < 0) { return -1; }

  char* buffer = malloc((size_t)len + 1);
  if (buffer == NULL) {
    errno = ENOMEM;
    return -1;
  }

  int ret = vsnprintf(buffer, (size_t)len + 1, format, parameters);
  if (ret < 0) {
    free(buffer);
    return -1;
  }

#if defined(__is_libk)
  for (int i = 0; i < ret; ++i) {
    if (fputc(buffer[i], stream) == EOF) {
      free(buffer);
      return -1;
    }
  }
  size_t written = (size_t)ret;
#else
  size_t written = fwrite(buffer, 1, (size_t)ret, stream);
#endif
  free(buffer);
  return written == (size_t)ret ? ret : -1;
}
