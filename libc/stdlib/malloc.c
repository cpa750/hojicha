#include <stdlib.h>
#include <hmalloc.h>

void* malloc(size_t size) {
  return hmalloc(size);
}
