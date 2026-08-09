#include <stdarg.h>
#include <stdio.h>

int fprintf(FILE* restrict stream, const char* restrict format, ...) {
  va_list parameters;
  va_start(parameters, format);
  int ret = vfprintf(stream, format, parameters);
  va_end(parameters);
  return ret;
}
