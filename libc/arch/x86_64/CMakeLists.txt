set(LIBC_ARCH_SOURCES
  memcpy.asm
  __syscalls.asm
)

target_sources(libc_objects PRIVATE ${LIBC_ARCH_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_ARCH_SOURCES})
