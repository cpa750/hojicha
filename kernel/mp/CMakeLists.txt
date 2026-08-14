target_sources(kernel_objects PRIVATE
  bootmodule.c
  mutex.c
  scheduler_ast.c
  scheduler.c
  semaphore.c
  spinlock.c
  syscall.c
  wait_queue.c
)
