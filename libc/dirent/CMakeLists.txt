set(LIBC_DIRENT_SOURCES
  getdents.c
)

target_sources(libc_objects PRIVATE ${LIBC_DIRENT_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_DIRENT_SOURCES})
