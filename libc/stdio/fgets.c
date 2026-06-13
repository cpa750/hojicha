#include <stdio.h>

char* fgets(char* restrict s, int n, FILE* restrict stream) {
  if (s == NULL || stream == NULL || n <= 0) { return NULL; }

  if (n == 1) {
    s[0] = '\0';
    return s;
  }

  int i = 0;
  while (i < n - 1) {
    int c = fgetc(stream);
    if (c == EOF) { break; }

    s[i++] = (char)c;
    if (c == '\n') { break; }
  }

  if (i == 0) { return NULL; }

  s[i] = '\0';
  return s;
}
