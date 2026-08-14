#include "mp_utils.h"

#include <kernel/g_kernel.h>
#include <mp/scheduler.h>

proc_queue_t* mp_ready_queue(void) {
  return &g_kernel.sched->ready_to_run;
}

proc_queue_t* mp_ready_to_die_queue(void) {
  return &g_kernel.sched->ready_to_die;
}

proc_queue_t* mp_sleeping_queue(void) {
  return &g_kernel.sched->sleeping;
}

void mp_block_proc(proc_t* p, uint8_t reason) {
  sched_lock();
  p->status = reason;
  schedule_advance();
  sched_unlock();
}

void mp_enqueue_ready(proc_t* process) {
  proc_queue_push_tail(mp_ready_queue(), process);
}

void mp_ready_to_die_remove(proc_t* target) {
  proc_queue_remove(mp_ready_to_die_queue(), target);
}

void mp_remove_proc(proc_t* p) {
  proc_queue_t* queue = NULL;
  switch (p->status) {
    case PROC_STATUS_READY_TO_RUN:
    case PROC_STATUS_UNINITIALIZED:
      queue = mp_ready_queue();
      break;
    case PROC_STATUS_SLEEPING_TIMER:
      queue = mp_sleeping_queue();
      break;
    default:
      return;
  }

  proc_queue_remove(queue, p);
}
