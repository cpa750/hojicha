#include <hmalloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void* realloc(void* ptr, size_t size) {
  if (ptr == NULL) { return malloc(size); }

  if (size == 0) {
    free(ptr);
    return NULL;
  }

  size_t old_size = hmalloc_usable_size(ptr);
  if (old_size >= size) { return ptr; }

  void* new_ptr = malloc(size);
  if (new_ptr == NULL) { return NULL; }

  if (old_size > 0) { memcpy(new_ptr, ptr, old_size); }
  free(ptr);
  return new_ptr;
}
