#include <internal/__hlibc_heap.h>
#include <internal/__hlibc_prologue.h>

void __hlibc_prologue(void) {
  __hlibc_heap_init();
  return;
}

