#include <hlog.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>

unsigned long syscall_nanosleep(unsigned long ns) {
  hlog_write(HLOG_DEBUG,
             "Sleeping process %s (PID: %d) for %d ns",
             sched_pb_get_name(g_kernel.current_process),
             sched_pb_get_pid(g_kernel.current_process),
             ns);
  sched_current_sleep_ns(ns);
  return 0;
}

