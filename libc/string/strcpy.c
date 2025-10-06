#include <stddef.h>
#include <string.h>

void* strcpy(void* restrict dest, const void* restrict src) {
  size_t len = strlen((const char*)src);
  return memcpy(dest, src, len);
}

