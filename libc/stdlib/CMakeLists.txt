set(LIBC_STDLIB_SOURCES
  abort.c
  abs.c
  assert.c
  atoi.c
  calloc.c
  free.c
  getenv.c
  itoa.c
  utoa.c
  malloc.c
  realloc.c
  exit.c
)

set(LIBC_STDLIB_LIBC_ONLY_SOURCES
  atof.c
)

target_sources(libc_objects PRIVATE ${LIBC_STDLIB_SOURCES})
target_sources(libc_objects PRIVATE ${LIBC_STDLIB_LIBC_ONLY_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_STDLIB_SOURCES})
