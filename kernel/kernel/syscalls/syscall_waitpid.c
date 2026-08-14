#include <errno.h>
#include <kernel/g_kernel.h>
#include <mp/wait.h>
#include <kernel/syscall_callbacks.h>
#include <kernel/syscall_utils.h>
#include <stddef.h>

long syscall_waitpid(long pid, int* wstatus, int options) {
  if (wstatus != NULL && !syscall_is_uaddr(wstatus, sizeof(int))) {
    return -EINVAL;
  }

  return waitpid_proc(g_kernel.current_process, pid, wstatus, options);
}
