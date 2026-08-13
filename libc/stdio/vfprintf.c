#include <errno.h>
#include <internal/__stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

static int fprintf_write(void* ctx, const char* data, size_t len) {
  FILE* stream = (FILE*)ctx;

#if defined(__is_libk)
  for (size_t i = 0; i < len; ++i) {
    if (fputc(data[i], stream) == EOF) { return -1; }
  }
  return 0;
#else
  return fwrite(data, 1, len, stream) == len ? 0 : -1;
#endif
}

int vfprintf(FILE* restrict stream,
             const char* restrict format,
             va_list parameters) {
  if (stream == NULL || format == NULL) {
    errno = EINVAL;
    return -1;
  }

  return __hojicha_vformat(fprintf_write, stream, format, parameters);
}
