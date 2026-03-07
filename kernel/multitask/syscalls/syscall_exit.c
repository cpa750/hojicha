#include <hlog.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>
#include <stdlib.h>

long syscall_exit(int code) {
  if (sched_pb_get_pid(g_kernel.current_process) ==
      sched_state_get_kernel_pid(g_kernel.sched)) {
    hlog_write(HLOG_FATAL, "Kernel called exit. Aborting...");
    abort();
  }

  hlog_add(HLOG_WARN,
           "Exit called from process \"%s\" (PID: %d) with code %d",
           sched_pb_get_name(g_kernel.current_process),
           sched_pb_get_pid(g_kernel.current_process),
           code);
  sched_proc_terminate(g_kernel.current_process);
  return 0;
}

