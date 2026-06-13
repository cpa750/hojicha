set(LIBC_INTERNAL_SOURCES
  __hlibc_heap.c
  __hlibc_prologue.c
  __syscalls.asm
)

target_sources(libc_objects PRIVATE ${LIBC_INTERNAL_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_INTERNAL_SOURCES})
