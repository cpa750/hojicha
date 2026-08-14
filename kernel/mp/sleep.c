#include <drivers/pit.h>
#include <kernel/g_kernel.h>
#include <mp/scheduler.h>
#include <mp/sleep.h>

#include "mp_utils.h"

static proc_t* find_last_sleep_timestamp_less_than_equal(proc_t* process,
                                                         uint64_t timestamp);

void block_current_proc(uint8_t reason) {
  mp_block_proc(g_kernel.current_process, reason);
}

void wake_proc(proc_t* process) {
  if (process == NULL) { return; }

  sched_lock();

  process->status = PROC_STATUS_READY_TO_RUN;
  process->next = NULL;

  if (g_kernel.sched->first_ready_to_run == NULL) {
    g_kernel.sched->first_ready_to_run = process;
    g_kernel.sched->last_ready_to_run = process;
  } else {
    if (g_kernel.sched->last_ready_to_run == NULL) {
      mp_set_last_ready_to_run(g_kernel.sched,
                               g_kernel.sched->first_ready_to_run);
    }
    g_kernel.sched->last_ready_to_run->next = process;
    g_kernel.sched->last_ready_to_run = process;
  }

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

  if (g_kernel.sched->sleeping == NULL) {
    process->next = NULL;
    g_kernel.sched->sleeping = process;
    sched_lock();
    schedule_advance();
    sched_unlock();
    sched_resume();
    return;
  }

  proc_t* last = find_last_sleep_timestamp_less_than_equal(
      g_kernel.sched->sleeping, timestamp);
  if (last == NULL) {
    process->next = g_kernel.sched->sleeping;
    g_kernel.sched->sleeping = process;
  } else {
    mp_insert_process_after(process, last);
  }

  sched_lock();
  schedule_advance();
  sched_unlock();
  sched_resume();
}

void wake_procs_before_timestamp(uint64_t timestamp) {
  if (g_kernel.sched->sleeping == NULL ||
      g_kernel.sched->sleeping->sleep_until > timestamp) {
    return;
  }

  proc_t* last_to_wake = NULL;
  for (proc_t* curr = g_kernel.sched->sleeping; curr != NULL;
       curr = curr->next) {
    if (curr->sleep_until <= timestamp) {
      curr->status = PROC_STATUS_READY_TO_RUN;
      last_to_wake = curr;
    } else {
      break;
    }
  }

  if (g_kernel.sched->first_ready_to_run == NULL) {
    g_kernel.sched->first_ready_to_run = g_kernel.sched->sleeping;
    g_kernel.sched->last_ready_to_run = last_to_wake;
  } else {
    g_kernel.sched->last_ready_to_run->next = g_kernel.sched->sleeping;
    g_kernel.sched->last_ready_to_run = last_to_wake;
  }

  g_kernel.sched->sleeping = last_to_wake->next;
  g_kernel.sched->last_ready_to_run->next = NULL;
}

static proc_t* find_last_sleep_timestamp_less_than_equal(proc_t* process,
                                                         uint64_t timestamp) {
  if (process->sleep_until > timestamp) { return NULL; }
  proc_t* ret = NULL;
  for (proc_t* curr = process; curr != NULL; curr = curr->next) {
    if (curr->sleep_until <= timestamp) {
      ret = curr;
    } else {
      break;
    }
  }
  return ret;
}
