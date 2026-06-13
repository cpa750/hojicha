#include <internal/__hlibc_heap.h>
#include <stdint.h>
#include <unistd.h>

void* sbrk(intptr_t offset) { return __hlibc_sbrk(offset); }

