set(LIBC_STRING_SOURCES
  memcmp.c
  memmove.c
  memset.c
  strcmp.c
  strcpy.c
  strlen.c
)

target_sources(libc_objects PRIVATE ${LIBC_STRING_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_STRING_SOURCES})
