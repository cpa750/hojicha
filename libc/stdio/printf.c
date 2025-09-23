#include <drivers/serial.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool print(const char* data, size_t length) {
  const unsigned char* raw_bytes = (const unsigned char*)data;
  for (size_t i = 0; i < length; i++) {
    // TODO: put serial write behind a compile time feature flag
    serial_write_char(raw_bytes[i]);
    if (putchar(raw_bytes[i]) == EOF) {
      return false;
    }
  }
  return true;
}

int printf(const char* restrict format, ...) {
  va_list parameters;
  va_start(parameters, format);

  int bytes_written = 0;

  while (*format != '\0') {
    size_t writeable_bytes = INT_MAX - bytes_written;
    if (format[0] != '%' || format[1] == '%') {
      if (format[0] == '%') {
        ++format;
      }
      size_t bytes_to_write = 1;
      while (format[bytes_to_write] && format[bytes_to_write] != '%') {
        ++bytes_to_write;
      }
      if (writeable_bytes < bytes_to_write) {
        return -1;
      }
      if (!print(format, bytes_to_write)) {
        return -1;
      }
      format += bytes_to_write;
      bytes_written += bytes_to_write;
    }
    const char* format_delimeter = format++;
    switch (*format) {
      case 'c': {
        format++;
        char c = (char)va_arg(parameters, int);
        if (!writeable_bytes) {
          return -1;
        }
        if (!print(&c, sizeof(c))) {
          return -1;
        }
        bytes_written++;
        break;
      }
      case 's': {
        format++;
        const char* s = (const char*)va_arg(parameters, const char*);
        size_t len = strlen(s);
        if (writeable_bytes < len) {
          return -1;
        }
        if (!print(s, len)) {
          return -1;
        }
        bytes_written += len;
        break;
      }
      case 'd': {
        format++;
        const uint32_t d = (const uint32_t)va_arg(parameters, const uint32_t);
        char buf[33];
        itoa(d, buf, 10);
        size_t len = strlen(buf + 2);
        if (writeable_bytes < len) {
          return -1;
        }
        if (!print(buf + 2, len)) {
          return -1;
        }
        bytes_written += len;
        break;
      }
      case 'x': {
        format++;
        const uint32_t x = (const uint32_t)va_arg(parameters, const uint32_t);
        char buf[33];
        itoa(x, buf, 16);
        size_t len = strlen(buf);
        if (writeable_bytes < len) {
          return -1;
        }
        if (!print(buf, len)) {
          return -1;
        }
        bytes_written += len;
        break;
      }
      case 'b': {
        format++;
        const uint32_t x = (const uint32_t)va_arg(parameters, const uint32_t);
        char buf[33];
        itoa(x, buf, 2);
        size_t len = strlen(buf);
        if (writeable_bytes < len) {
          return -1;
        }
        if (!print(buf, len)) {
          return -1;
        }
        bytes_written += len;
        break;
      }
      default: {
        format = format_delimeter;
        size_t len = strlen(format);
        if (writeable_bytes < len) {
          return -1;
        }
        if (!print(format, len)) {
          return -1;
        }
        bytes_written += len;
        format += len;
        break;
      }
    }
  }
  va_end(parameters);
  return bytes_written;
}

