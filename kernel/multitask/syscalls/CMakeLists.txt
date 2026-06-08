target_sources(kernel_objects PRIVATE
  syscall_close.c
  syscall_exit.c
  syscall_fork.c
  syscall_getdents.c
  syscall_ioctl.c
  syscall_lseek.c
  syscall_mkdir.c
  syscall_nanosleep.c
  syscall_open.c
  syscall_read.c
  syscall_rmdir.c
  syscall_stat.c
  syscall_unlink.c
  syscall_utils.c
  syscall_write.c
)
