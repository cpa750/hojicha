#include <hlog.h>
#include <kernel/g_kernel.h>
#include <mp/proc.h>
#include <mp/scheduler.h>
#include <kernel/syscall_callbacks.h>
#include <stdlib.h>

long syscall_exit(int code) {
  if (proc_get_pid(g_kernel.current_process) ==
      sched_state_get_kernel_pid(g_kernel.sched)) {
    hlog_write(HLOG_FATAL, "Kernel called exit. Aborting...");
    abort();
  }

  hlog_add(HLOG_WARN,
           "Exit called from process \"%s\" (PID: %d) with code %d",
           proc_get_name(g_kernel.current_process),
           proc_get_pid(g_kernel.current_process),
           code);
  sched_proc_exit(g_kernel.current_process, code);
  return 0;
}
