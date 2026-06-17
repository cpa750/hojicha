#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int append(char* buffer,
                  size_t size,
                  int* bytes_written,
                  const char* data,
                  size_t len) {
  if ((size_t)(INT_MAX - *bytes_written) < len) { return -1; }

  for (size_t i = 0; i < len; ++i) {
    if (size > 0 && (size_t)*bytes_written < size - 1) {
      buffer[*bytes_written] = data[i];
    }
    ++*bytes_written;
  }

  return 0;
}

int vsnprintf(char* restrict buffer,
              size_t size,
              const char* restrict format,
              va_list parameters) {
  int bytes_written = 0;

  while (*format != '\0') {
    if (format[0] != '%' || format[1] == '%') {
      if (format[0] == '%') { ++format; }

      size_t bytes_to_write = 1;
      while (format[bytes_to_write] != '\0' && format[bytes_to_write] != '%') {
        ++bytes_to_write;
      }

      if (append(buffer, size, &bytes_written, format, bytes_to_write) < 0) {
        return -1;
      }

      format += bytes_to_write;
      if (*format == '\0') { break; }
    }

    const char* format_delimiter = format++;
    switch (*format) {
      case 'c': {
        ++format;
        char c = (char)va_arg(parameters, int);
        if (append(buffer, size, &bytes_written, &c, sizeof(c)) < 0) {
          return -1;
        }
        break;
      }
      case 's': {
        ++format;
        const char* s = (const char*)va_arg(parameters, const char*);
        if (s == NULL) { s = "(null)"; }
        if (append(buffer, size, &bytes_written, s, strlen(s)) < 0) {
          return -1;
        }
        break;
      }
      case 'd': {
        ++format;
        const uint64_t d = (const uint64_t)va_arg(parameters, const uint64_t);
        char buf[40];
        itoa(d, buf, 10);
        if (append(buffer, size, &bytes_written, buf + 2, strlen(buf + 2)) < 0) {
          return -1;
        }
        break;
      }
      case 'x': {
        ++format;
        const uint64_t x = (const uint64_t)va_arg(parameters, const uint64_t);
        char buf[40];
        utoa(x, buf, 16);
        if (append(buffer, size, &bytes_written, buf, strlen(buf)) < 0) {
          return -1;
        }
        break;
      }
      case 'b': {
        ++format;
        const uint32_t x = (const uint32_t)va_arg(parameters, const uint32_t);
        char buf[40];
        itoa(x, buf, 2);
        if (append(buffer, size, &bytes_written, buf, strlen(buf)) < 0) {
          return -1;
        }
        break;
      }
      default: {
        format = format_delimiter;
        size_t len = strlen(format);
        if (append(buffer, size, &bytes_written, format, len) < 0) {
          return -1;
        }
        format += len;
        break;
      }
    }
  }

  if (size > 0) {
    size_t terminator = (size_t)bytes_written < size ? (size_t)bytes_written : size - 1;
    buffer[terminator] = '\0';
  }

  return bytes_written;
}
