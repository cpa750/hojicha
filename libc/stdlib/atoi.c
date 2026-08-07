#include <ctype.h>
#include <stdlib.h>

int atoi(const char* nptr) {
  int sign = 1;
  int value = 0;

  while (isspace((unsigned char)*nptr)) { ++nptr; }

  if (*nptr == '+' || *nptr == '-') {
    if (*nptr == '-') { sign = -1; }
    ++nptr;
  }

  while (*nptr >= '0' && *nptr <= '9') {
    value = value * 10 + (*nptr - '0');
    ++nptr;
  }

  return sign * value;
}
