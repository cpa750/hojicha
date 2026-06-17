#include <string.h>

char* strtok_r(char* restrict str,
               const char* restrict delim,
               char** restrict saveptr) {
  char* token = str != NULL ? str : *saveptr;
  if (token == NULL) { return NULL; }

  while (*token != '\0' && strchr(delim, *token) != NULL) { ++token; }
  if (*token == '\0') {
    *saveptr = token;
    return NULL;
  }

  char* end = token;
  while (*end != '\0' && strchr(delim, *end) == NULL) { ++end; }

  if (*end != '\0') {
    *end = '\0';
    *saveptr = end + 1;
  } else {
    *saveptr = end;
  }

  return token;
}
