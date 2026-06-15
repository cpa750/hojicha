#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

int snprintf(char* restrict buffer, size_t size, const char* restrict format, ...) {
  va_list parameters;
  va_start(parameters, format);
  int bytes_written = vsnprintf(buffer, size, format, parameters);
  va_end(parameters);
  return bytes_written;
}
