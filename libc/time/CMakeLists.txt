set(LIBC_TIME_SOURCES
  clock_gettime.c
  nanosleep.c
)

target_sources(libc_objects PRIVATE ${LIBC_TIME_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_TIME_SOURCES})
