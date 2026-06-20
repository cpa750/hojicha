set(LIBC_UNISTD_SOURCES
  access.c
  brk.c
  close.c
  execve.c
  fork.c
  link.c
  lseek.c
  readlink.c
  rmdir.c
  sleep.c
  sbrk.c
  symlink.c
  unlink.c
)

target_sources(libc_objects PRIVATE ${LIBC_UNISTD_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_UNISTD_SOURCES})
