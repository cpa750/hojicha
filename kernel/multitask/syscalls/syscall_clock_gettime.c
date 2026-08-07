#include <errno.h>
#include <kernel/ktime.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
#include <time.h>

long syscall_clock_gettime(clockid_t clockid, struct timespec* tp) {
  if (tp == NULL) { return -EINVAL; }
  if (!syscall_is_uaddr(tp, sizeof(struct timespec))) { return -EINVAL; }

  if (clockid != CLOCK_MONOTONIC && clockid != CLOCK_REALTIME) {
    return -EINVAL;
  }

  if (clockid == CLOCK_MONOTONIC) {
    tp->tv_sec = (int64_t)uptime();
  } else if (clockid == CLOCK_REALTIME) {
    tp->tv_sec = (int64_t)unix_time();
  }
  tp->tv_nsec = 0;

  return 0;
}
