#include <ctype.h>
#include <strings.h>

int strcasecmp(const char* a, const char* b) {
  for (;;) {
    unsigned char ac = (unsigned char)tolower((unsigned char)*a);
    unsigned char bc = (unsigned char)tolower((unsigned char)*b);
    if (ac != bc || ac == '\0') { return ac - bc; }
    ++a;
    ++b;
  }
}
