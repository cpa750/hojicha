#include <internal/__hlibc_heap.h>
#include <unistd.h>

int brk(void* addr) { return __hlibc_brk(addr); }

