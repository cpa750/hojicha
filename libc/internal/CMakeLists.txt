set(LIBC_HOJICHA_SYSDEP_SOURCES
  __hlibc_heap.c
  __hlibc_prologue.c
  __syscalls.asm
)

target_sources(libc_objects PRIVATE ${LIBC_HOJICHA_SYSDEP_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_HOJICHA_SYSDEP_SOURCES})
