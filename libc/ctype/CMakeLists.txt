set(LIBC_CTYPE_SOURCES
  isprint.c
  isspace.c
  tolower.c
  toupper.c
)

target_sources(libc_objects PRIVATE ${LIBC_CTYPE_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_CTYPE_SOURCES})
