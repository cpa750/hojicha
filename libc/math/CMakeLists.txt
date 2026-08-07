set(LIBC_MATH_SOURCES
  fabs.c
)

target_sources(libc_objects PRIVATE ${LIBC_MATH_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_MATH_SOURCES})
