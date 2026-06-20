set(LIBC_UNISTD_SOURCES
  access.c
  brk.c
  chdir.c
  close.c
  dup2.c
  execve.c
  fork.c
  link.c
  lseek.c
  rmdir.c
  sleep.c
  sbrk.c
)

target_sources(libc_objects PRIVATE ${LIBC_UNISTD_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_UNISTD_SOURCES})
