#include <stdlib.h>
#if defined(__is_libk)
#include <memory/kmalloc.h>
#endif

void* malloc(size_t size) {
#if defined(__is_libk)
  return kmalloc(size);
#else
  // TODO userspace malloc()
  return 0;
#endif
}

