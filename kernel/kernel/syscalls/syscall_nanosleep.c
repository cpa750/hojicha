#include <hlog.h>
#include <kernel/g_kernel.h>
#include <mp/proc.h>
#include <mp/sleep.h>
#include <kernel/syscall_callbacks.h>

unsigned long syscall_nanosleep(unsigned long ns) {
  hlog_write(HLOG_DEBUG,
             "Sleeping process %s (PID: %d) for %d ns",
             proc_get_name(g_kernel.current_process),
             proc_get_pid(g_kernel.current_process),
             ns);
  sleep_current_ns(ns);
  return 0;
}
