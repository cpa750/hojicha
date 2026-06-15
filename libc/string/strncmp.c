#include <stddef.h>
#include <string.h>

int strncmp(const char* a, const char* b, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    unsigned char ac = (unsigned char)a[i];
    unsigned char bc = (unsigned char)b[i];
    if (ac != bc) { return ac - bc; }
    if (ac == '\0') { return 0; }
  }

  return 0;
}
