#include <stdlib.h>
#include <hmalloc.h>

void free(void* ptr) {
  hfree(ptr);
}
