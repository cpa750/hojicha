target_sources(kernel_objects PRIVATE
  bootmodule.c
  elf.c
  mutex.c
  scheduler_ast.c
  scheduler.c
  semaphore.c
  syscall.c
  wait_queue.c
)

add_subdirectory(syscalls)
