#include <limits.h>
#include <memory/kmalloc.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print(char* buffer, const char* data, uint64_t len) {
  memcpy(buffer, data, len);
}

// TODO: Figure out how to make vprintf use this instead of re-implementing
// the same logic
int vsnprintf(char* buffer, const char* format, va_list parameters) {
  int bytes_written = 0;

  while (*format != '\0') {
    size_t writeable_bytes = INT_MAX - bytes_written;
    if (format[0] != '%' || format[1] == '%') {
      if (format[0] == '%') { ++format; }
      size_t bytes_to_write = 1;
      while (format[bytes_to_write] && format[bytes_to_write] != '%') {
        ++bytes_to_write;
      }
      if (writeable_bytes < bytes_to_write) { return -1; }
      print(buffer, format, bytes_to_write);
      buffer += bytes_to_write;

      format += bytes_to_write;
      bytes_written += bytes_to_write;

      // Needed to prevent to prevent format reading in bogus values
      if (*format == '\0') { break; }
    }
    const char* format_delimeter = format++;
    switch (*format) {
      case 'c': {
        format++;
        char c = (char)va_arg(parameters, int);
        if (!writeable_bytes) { return -1; }
        print(buffer, &c, sizeof(char));
        buffer++;
        bytes_written++;
        break;
      }
      case 's': {
        format++;
        const char* s = (const char*)va_arg(parameters, const char*);
        size_t len = strlen(s);
        if (writeable_bytes < len) { return -1; }
        print(buffer, s, len);
        buffer += len;
        bytes_written += len;
        break;
      }
      case 'd': {
        format++;
        const uint64_t d = (const uint64_t)va_arg(parameters, const uint64_t);
        char buf[40];
        itoa(d, buf, 10);
        size_t len = strlen(buf + 2);
        if (writeable_bytes < len) { return -1; }
        print(buffer, buf + 2, len);
        buffer += len;
        bytes_written += len;
        break;
      }
      case 'x': {
        // TODO: Fix garbled output when parameter is >= 0xF0000000
        format++;
        const uint64_t x = (const uint64_t)va_arg(parameters, const uint64_t);
        char buf[40];
        utoa(x, buf, 16);
        size_t len = strlen(buf);
        if (writeable_bytes < len) { return -1; }
        print(buffer, buf, len);
        buffer += len;
        bytes_written += len;
        break;
      }
      case 'b': {
        format++;
        const uint32_t x = (const uint32_t)va_arg(parameters, const uint32_t);
        char buf[40];
        itoa(x, buf, 2);
        size_t len = strlen(buf);
        if (writeable_bytes < len) { return -1; }
        print(buffer, buf, len);
        buffer += len;
        bytes_written += len;
        break;
      }
      default: {
        format = format_delimeter;
        size_t len = strlen(format);
        if (writeable_bytes < len) { return -1; }
        print(buffer, format, len);
        buffer += len;
        bytes_written += len;
        format += len;
        break;
      }
    }
  }
  buffer[0] = '\0';
  return bytes_written;
}

