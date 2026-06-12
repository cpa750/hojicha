#include <stddef.h>
#include <string.h>

void* strcpy(void* restrict dest, const void* restrict src) {
  size_t len = strlen((const char*)src) + 1;
  return memcpy(dest, src, len);
}
