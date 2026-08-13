#include <internal/__stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct format_spec {
  int width;
  int precision;
  bool precision_set;
  bool zero_pad;
  char conversion;
};

static int write_all(__hojicha_printf_write_fn write,
                     void* ctx,
                     int* bytes_written,
                     const char* data,
                     size_t len) {
  if ((size_t)(INT_MAX - *bytes_written) < len) { return -1; }
  if (len > 0 && write(ctx, data, len) < 0) { return -1; }
  *bytes_written += (int)len;
  return 0;
}

static bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

static const char* parse_format_spec(const char* format,
                                     struct format_spec* spec) {
  spec->width = 0;
  spec->precision = 0;
  spec->precision_set = false;
  spec->zero_pad = false;

  if (*format == '0') {
    spec->zero_pad = true;
    ++format;
  }

  while (is_digit(*format)) {
    spec->width = (spec->width * 10) + (*format - '0');
    ++format;
  }

  if (*format == '.') {
    spec->precision_set = true;
    ++format;
    while (is_digit(*format)) {
      spec->precision = (spec->precision * 10) + (*format - '0');
      ++format;
    }
  }

  spec->conversion = *format;
  if (*format != '\0') { ++format; }
  return format;
}

static size_t format_unsigned(uint64_t value, unsigned int base, char* out) {
  char tmp[65];
  size_t len = 0;

  if (value == 0) {
    tmp[len++] = '0';
  } else {
    while (value != 0) {
      unsigned int digit = value % base;
      tmp[len++] = "0123456789abcdef"[digit];
      value /= base;
    }
  }

  for (size_t i = 0; i < len; ++i) { out[i] = tmp[len - i - 1]; }
  out[len] = '\0';
  return len;
}

static int write_repeated(__hojicha_printf_write_fn write,
                          void* ctx,
                          int* bytes_written,
                          char c,
                          size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (write_all(write, ctx, bytes_written, &c, 1) < 0) { return -1; }
  }
  return 0;
}

static int write_formatted_number(__hojicha_printf_write_fn write,
                                  void* ctx,
                                  int* bytes_written,
                                  const struct format_spec* spec,
                                  bool is_signed,
                                  uint64_t value) {
  char digits[65];
  bool negative = false;

  if (is_signed && (int64_t)value < 0) {
    negative = true;
    value = (uint64_t)(-(int64_t)value);
  }

  size_t digit_len =
      format_unsigned(value, spec->conversion == 'x' ? 16 : 10, digits);
  size_t zero_count = 0;
  if (spec->precision_set && spec->precision > (int)digit_len) {
    zero_count = (size_t)spec->precision - digit_len;
  }

  size_t sign_len = negative ? 1 : 0;
  size_t content_len = sign_len + zero_count + digit_len;
  size_t pad_count = 0;
  if (spec->width > (int)content_len) {
    pad_count = (size_t)spec->width - content_len;
  }

  if (spec->zero_pad && !spec->precision_set) {
    zero_count += pad_count;
    pad_count = 0;
  }

  if (write_repeated(write, ctx, bytes_written, ' ', pad_count) < 0) {
    return -1;
  }
  if (negative) {
    char sign = '-';
    if (write_all(write, ctx, bytes_written, &sign, 1) < 0) { return -1; }
  }
  if (write_repeated(write, ctx, bytes_written, '0', zero_count) < 0) {
    return -1;
  }
  return write_all(write, ctx, bytes_written, digits, digit_len);
}

int __hojicha_vformat(__hojicha_printf_write_fn write,
                      void* ctx,
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

      if (write_all(write, ctx, &bytes_written, format, bytes_to_write) < 0) {
        return -1;
      }

      format += bytes_to_write;
      if (*format == '\0') { break; }
    }

    const char* format_delimiter = format++;
    struct format_spec spec;
    format = parse_format_spec(format, &spec);

    switch (spec.conversion) {
      case 'c': {
        char c = (char)va_arg(parameters, int);
        if (write_all(write, ctx, &bytes_written, &c, sizeof(c)) < 0) {
          return -1;
        }
        break;
      }
      case 's': {
        const char* s = (const char*)va_arg(parameters, const char*);
        if (s == NULL) { s = "(null)"; }
        if (write_all(write, ctx, &bytes_written, s, strlen(s)) < 0) {
          return -1;
        }
        break;
      }
      case 'd':
      case 'i': {
        const uint64_t d = va_arg(parameters, uint64_t);
        if (write_formatted_number(
                write, ctx, &bytes_written, &spec, true, d) < 0) {
          return -1;
        }
        break;
      }
      case 'x': {
        const uint64_t x = va_arg(parameters, uint64_t);
        if (write_formatted_number(
                write, ctx, &bytes_written, &spec, false, x) < 0) {
          return -1;
        }
        break;
      }
      case 'u': {
        const uint64_t u = va_arg(parameters, uint64_t);
        if (write_formatted_number(
                write, ctx, &bytes_written, &spec, false, u) < 0) {
          return -1;
        }
        break;
      }
      case 'b': {
        const uint32_t x = (const uint32_t)va_arg(parameters, const uint32_t);
        char buf[40];
        itoa(x, buf, 2);
        if (write_all(write, ctx, &bytes_written, buf, strlen(buf)) < 0) {
          return -1;
        }
        break;
      }
      default: {
        format = format_delimiter;
        size_t len = strlen(format);
        if (write_all(write, ctx, &bytes_written, format, len) < 0) {
          return -1;
        }
        format += len;
        break;
      }
    }
  }

  return bytes_written;
}
