target_sources(kernel_objects PRIVATE
  bootmodule.c
  elf.c
  mutex.c
  scheduler.c
  semaphore.c
  syscall.c
)

add_subdirectory(syscalls)
