set(LIBC_MATH_SOURCES
  fabs.c
)

target_sources(libc_objects PRIVATE ${LIBC_MATH_SOURCES})
