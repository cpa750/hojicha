set(LIBC_STRING_SOURCES
  memcmp.c
  memmove.c
  memset.c
  strchr.c
  strcasecmp.c
  strcmp.c
  strncasecmp.c
  strncmp.c
  strncpy.c
  strrchr.c
  strcpy.c
  strdup.c
  strlen.c
  strtok.c
  strtok_r.c
)

target_sources(libc_objects PRIVATE ${LIBC_STRING_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_STRING_SOURCES})
