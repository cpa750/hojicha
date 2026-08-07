set(LIBC_STDLIB_SOURCES
  abort.c
  abs.c
  assert.c
  atoi.c
  atof.c
  calloc.c
  free.c
  getenv.c
  itoa.c
  utoa.c
  malloc.c
  realloc.c
  exit.c
)

target_sources(libc_objects PRIVATE ${LIBC_STDLIB_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_STDLIB_SOURCES})
