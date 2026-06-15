set(LIBC_STDIO_SOURCES
  fgetc.c
  fgets.c
  fputc.c
  getchar.c
  printf.c
  putchar.c
  puts.c
  snprintf.c
  streams.c
  vprintf.c
  vsnprintf.c
)

target_sources(libc_objects PRIVATE ${LIBC_STDIO_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_STDIO_SOURCES})
