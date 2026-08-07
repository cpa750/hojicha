#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

static bool digit_value(char c, int base, int* value_out) {
  int value;
  if (c >= '0' && c <= '9') {
    value = c - '0';
  } else if (c >= 'a' && c <= 'f') {
    value = c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    value = c - 'A' + 10;
  } else {
    return false;
  }

  if (value >= base) { return false; }
  *value_out = value;
  return true;
}

static bool scan_int(const char** input_inout, int base, int* out) {
  const char* input = *input_inout;
  int sign = 1;
  unsigned int value = 0;
  bool saw_digit = false;

  if (*input == '+' || *input == '-') {
    if (*input == '-') { sign = -1; }
    ++input;
  }

  if ((base == 0 || base == 16) && input[0] == '0' &&
      (input[1] == 'x' || input[1] == 'X')) {
    base = 16;
    input += 2;
  } else if (base == 0 && input[0] == '0') {
    base = 8;
  } else if (base == 0) {
    base = 10;
  }

  while (*input != '\0') {
    int digit;
    if (!digit_value(*input, base, &digit)) { break; }
    value = value * (unsigned int)base + (unsigned int)digit;
    saw_digit = true;
    ++input;
  }

  if (!saw_digit) { return false; }
  *out = sign < 0 ? -(int)value : (int)value;
  *input_inout = input;
  return true;
}

int sscanf(const char* restrict str, const char* restrict format, ...) {
  if (str == NULL || format == NULL) {
    errno = EINVAL;
    return EOF;
  }

  va_list parameters;
  va_start(parameters, format);

  const char* input = str;
  const char* scan_format = format;
  int assigned = 0;

  while (*scan_format != '\0') {
    if (isspace((unsigned char)*scan_format)) {
      while (isspace((unsigned char)*scan_format)) { ++scan_format; }
      while (isspace((unsigned char)*input)) { ++input; }
      continue;
    }

    if (*scan_format != '%') {
      if (*input != *scan_format) { break; }
      ++scan_format;
      ++input;
      continue;
    }

    ++scan_format;
    int base;
    switch (*scan_format) {
      case 'd':
        base = 10;
        break;
      case 'i':
        base = 0;
        break;
      case 'o':
        base = 8;
        break;
      case 'x':
      case 'X':
        base = 16;
        break;
      default:
        va_end(parameters);
        errno = EINVAL;
        return assigned;
    }

    while (isspace((unsigned char)*input)) { ++input; }

    int* out = va_arg(parameters, int*);
    if (!scan_int(&input, base, out)) { break; }
    ++assigned;
    ++scan_format;
  }

  va_end(parameters);
  return assigned;
}
