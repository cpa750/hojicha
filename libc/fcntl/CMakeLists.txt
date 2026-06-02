set(LIBC_FCNTL_SOURCES
  open.c
  read.c
  write.c
)

target_sources(libc_objects PRIVATE ${LIBC_FCNTL_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_FCNTL_SOURCES})
