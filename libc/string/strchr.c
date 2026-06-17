#include <string.h>

char* strchr(const char* str, int c) {
  char ch = (char)c;

  while (*str != '\0') {
    if (*str == ch) { return (char*)str; }
    ++str;
  }

  return ch == '\0' ? (char*)str : NULL;
}
