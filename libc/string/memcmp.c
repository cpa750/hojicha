#include <string.h>

int memcmp(const void* a, const void* b, size_t size) {
  const unsigned char* first = (const unsigned char*)a;
  const unsigned char* second = (const unsigned char*)b;
  for (size_t i = 0; i < size; i++) {
    if (first[i] < second[i]) {
      return -1;
    } else if (first[i] > second[i]) {
      return 1;
    }
  }
  return 0;
}

