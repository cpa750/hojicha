#include <stdlib.h>
#if defined(__is_libk)
#include <memory/kmalloc.h>
#endif

void free(void* ptr) {
#if defined(__is_libk)
  return kfree(ptr);
#else
  // TODO userspace free()
  return;
#endif
}
