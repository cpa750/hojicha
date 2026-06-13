#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void* calloc(size_t count, size_t size) {
  if (count != 0 && size > ((size_t)-1) / count) { return NULL; }

  size_t total = count * size;
  void* ptr = malloc(total);
  if (ptr == NULL) { return NULL; }

  memset(ptr, 0, total);
  return ptr;
}
