#include <stddef.h>
#include <string.h>

void* memset(void* buffer, int value, size_t size) {
  unsigned char* buf = (unsigned char*)buffer;
  for (size_t i = 0; i < size; i++) { buf[i] = (unsigned char)value; }
  return buffer;
}
