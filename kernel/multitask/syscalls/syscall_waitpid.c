#include <errno.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
#include <stddef.h>

long syscall_waitpid(long pid, int* wstatus, int options) {
  if (wstatus != NULL && !syscall_is_uaddr(wstatus, sizeof(int))) {
    return -EINVAL;
  }

  return sched_waitpid(g_kernel.current_process, pid, wstatus, options);
}
