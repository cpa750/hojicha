#include <drivers/pit.h>
#include <kernel/g_kernel.h>
#include <mp/scheduler.h>
#include <mp/sleep.h>

#include "mp_utils.h"

static bool proc_sleep_until_less_than_equal(proc_t* process, void* ctx);

void block_current_proc(uint8_t reason) {
  mp_block_proc(g_kernel.current_process, reason);
}

void wake_proc(proc_t* process) {
  if (process == NULL) { return; }

  sched_lock();

  process->status = PROC_STATUS_READY_TO_RUN;
  proc_queue_push_tail(mp_ready_queue(), process);

  sched_unlock();
}

void sleep_current(uint64_t s) {
  sleep_current_ns(s * 1000000000ULL);
}

void sleep_current_ns(uint64_t ns) {
  sleep_proc_until(g_kernel.current_process,
                   pit_get_ns_elapsed_since_init(g_kernel.pit) + ns);
}

void sleep_proc_until(proc_t* process, uint64_t timestamp) {
  sched_postpone();
  process->sleep_until = timestamp;
  process->status = PROC_STATUS_SLEEPING_TIMER;

  if (proc_queue_empty(mp_sleeping_queue())) {
    proc_queue_push_head(mp_sleeping_queue(), process);
    sched_lock();
    schedule_advance();
    sched_unlock();
    sched_resume();
    return;
  }

  proc_t* last = proc_queue_find_last_prefix(
      mp_sleeping_queue(), proc_sleep_until_less_than_equal, &timestamp);
  if (last == NULL) {
    proc_queue_push_head(mp_sleeping_queue(), process);
  } else {
    proc_queue_insert_after(mp_sleeping_queue(), last, process);
  }

  sched_lock();
  schedule_advance();
  sched_unlock();
  sched_resume();
}

void wake_procs_before_timestamp(uint64_t timestamp) {
  proc_t* last_to_wake = proc_queue_find_last_prefix(
      mp_sleeping_queue(), proc_sleep_until_less_than_equal, &timestamp);
  if (last_to_wake == NULL) {
    return;
  }

  for (proc_t* curr = proc_queue_head(mp_sleeping_queue()); curr != NULL;
       curr = proc_get_next(curr)) {
    curr->status = PROC_STATUS_READY_TO_RUN;
    if (curr == last_to_wake) { break; }
  }

  proc_queue_splice_prefix_tail(mp_ready_queue(),
                                mp_sleeping_queue(),
                                last_to_wake);
}

static bool proc_sleep_until_less_than_equal(proc_t* process, void* ctx) {
  uint64_t timestamp = *(uint64_t*)ctx;
  return process->sleep_until <= timestamp;
}
