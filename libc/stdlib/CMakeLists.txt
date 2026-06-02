set(LIBC_STDLIB_SOURCES
  abort.c
  assert.c
  calloc.c
  free.c
  itoa.c
  utoa.c
  malloc.c
  exit.c
)

target_sources(libc_objects PRIVATE ${LIBC_STDLIB_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_STDLIB_SOURCES})
