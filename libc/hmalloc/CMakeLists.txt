set(LIBC_HMALLOC_SOURCES
  hmalloc.c
)

target_sources(libc_objects PRIVATE ${LIBC_HMALLOC_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_HMALLOC_SOURCES})
