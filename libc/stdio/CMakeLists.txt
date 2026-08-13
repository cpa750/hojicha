set(LIBC_STDIO_SOURCES
  __vformat.c
  fgetc.c
  fclose.c
  feof.c
  fflush.c
  ferror.c
  fgets.c
  fopen.c
  fputc.c
  fprintf.c
  fread.c
  fseek.c
  ftell.c
  fwrite.c
  getchar.c
  printf.c
  putchar.c
  puts.c
  remove.c
  rename.c
  snprintf.c
  sscanf.c
  vfprintf.c
  streams.c
  vprintf.c
  vsnprintf.c
)

target_sources(libc_objects PRIVATE ${LIBC_STDIO_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_STDIO_SOURCES})
