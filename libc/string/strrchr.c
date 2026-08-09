#include <stddef.h>
#include <string.h>

char* strrchr(const char* str, int c) {
  const char* last = NULL;
  char ch = (char)c;

  do {
    if (*str == ch) { last = str; }
  } while (*str++ != '\0');

  return (char*)last;
}
