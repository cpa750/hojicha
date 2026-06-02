set(LIBC_STDIO_SOURCES
  printf.c
  putchar.c
  puts.c
  vprintf.c
  vsnprintf.c
)

target_sources(libc_objects PRIVATE ${LIBC_STDIO_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_STDIO_SOURCES})
