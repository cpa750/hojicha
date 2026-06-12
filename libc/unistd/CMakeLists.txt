set(LIBC_UNISTD_SOURCES
  close.c
  execve.c
  fork.c
  lseek.c
  rmdir.c
  sleep.c
  unlink.c
)

target_sources(libc_objects PRIVATE ${LIBC_UNISTD_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_UNISTD_SOURCES})
