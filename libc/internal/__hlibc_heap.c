#include <errno.h>
#include <internal/__hlibc_heap.h>
#include <internal/__syscalls.h>
#include <stddef.h>

static unsigned long brk_start = 0;
static unsigned long current_brk = 0;

void __hlibc_heap_init(void) {
  brk_start = __syscall1(__HOJICHA_SYS_SYSCALL_BRK, 0);
  current_brk = brk_start;
  return;
}

int __hlibc_brk(void* addr) {
  if ((unsigned long)addr < current_brk) {
    errno = ENOMEM;
    return -1;
  }
  unsigned long new_brk =
      __syscall1(__HOJICHA_SYS_SYSCALL_BRK, (unsigned long)addr);
  if (new_brk == current_brk) {
    errno = ENOMEM;
    return -1;
  }
  current_brk = new_brk;
  return 0;
}

void* __hlibc_sbrk(intptr_t offset) {
  unsigned long old_brk = current_brk;
  if (offset < 0) {
    errno = ENOMEM;
    return (void*)-1;
  }

  unsigned long new_brk = __syscall1(__HOJICHA_SYS_SYSCALL_BRK,
                                     (unsigned long)current_brk + offset);
  if (new_brk == current_brk) {
    errno = ENOMEM;
    return (void*)-1;
  }
  current_brk = new_brk;
  return (void*)old_brk;
}

