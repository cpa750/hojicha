target_sources(kernel_objects PRIVATE
  bootmodule.c
  elf.c
  scheduler.c
  semaphore.c
  syscall.c
)

add_subdirectory(syscalls)
