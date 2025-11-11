#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

char* itoa(int64_t num, char* dst, int base) {
  if (num == 0) {
    dst[0] = '0';
    dst[1] = '\0';
    return dst;
  }

  bool is_negative = false;
  if (num < 0 && base == 10) {
    num = -num;
    is_negative = true;
  }

  size_t i = 0;
  while (num != 0) {
    int remainder = num % base;
    dst[i++] = (remainder > 9) ? (remainder - 10) + 'a' : remainder + '0';
    num /= base;
  }

  switch (base) {
    case 16:
      dst[i++] = 'x';
      break;
    case 10:
      dst[i++] = 'd';
      break;
    case 8:
      dst[i++] = 'o';
      break;
    case 2:
      dst[i++] = 'b';
      break;
  }

  dst[i++] = '0';

  if (is_negative && base == 10) {
    dst[i++] = '-';
  }

  dst[i] = '\0';

  size_t start = 0;
  size_t end = i - 1;
  while (start < end) {
    char temp = dst[end];
    dst[end] = dst[start];
    dst[start] = temp;
    end--;
    start++;
  }

  return dst;
}
