#include <string.h>

int strcmp(const char* a, const char* b) {
  for (; *a != '\0' && *a == *b; ++a, ++b) {}
  return (unsigned char)*a - (unsigned char)*b;
}
