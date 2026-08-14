target_sources(kernel_objects PRIVATE
  bootmodule.c
  execve.c
  fork.c
  mp_utils.c
  mutex.c
  proc.c
  proc_queue.c
  scheduler_ast.c
  scheduler.c
  semaphore.c
  sleep.c
  spinlock.c
  syscall.c
  wait.c
  wait_queue.c
)
