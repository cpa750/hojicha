#include <ctype.h>
#include <stddef.h>
#include <strings.h>

int strncasecmp(const char* a, const char* b, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    unsigned char ac = (unsigned char)tolower((unsigned char)a[i]);
    unsigned char bc = (unsigned char)tolower((unsigned char)b[i]);
    if (ac != bc) { return ac - bc; }
    if (ac == '\0') { return 0; }
  }

  return 0;
}
