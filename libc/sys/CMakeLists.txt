set(LIBC_SYS_SOURCES
  mkdir.c
  stat.c
)

target_sources(libc_objects PRIVATE ${LIBC_SYS_SOURCES})
target_sources(libk_objects PRIVATE ${LIBC_SYS_SOURCES})
