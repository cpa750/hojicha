#include "mp_utils.h"

#include <kernel/g_kernel.h>
#include <mp/scheduler.h>

void mp_block_proc(proc_t* p, uint8_t reason) {
  sched_lock();
  p->status = reason;
  schedule_advance();
  sched_unlock();
}

void mp_enqueue_ready(proc_t* process) {
  process->next = NULL;
  if (g_kernel.sched->first_ready_to_run == NULL) {
    g_kernel.sched->first_ready_to_run = process;
    g_kernel.sched->last_ready_to_run = process;
    return;
  }

  if (g_kernel.sched->last_ready_to_run == NULL) {
    mp_set_last_ready_to_run(g_kernel.sched,
                             g_kernel.sched->first_ready_to_run);
  }
  g_kernel.sched->last_ready_to_run->next = process;
  g_kernel.sched->last_ready_to_run = process;
}

void mp_insert_process_after(proc_t* process, proc_t* after) {
  if (after == NULL) { return; }

  process->next = after->next;
  after->next = process;
}

void mp_ready_to_die_remove(proc_t* target) {
  if (target == NULL) { return; }

  proc_t* prev = NULL;
  for (proc_t* p = g_kernel.sched->ready_to_die; p != NULL; p = p->next) {
    if (p != target) {
      prev = p;
      continue;
    }

    if (prev == NULL) {
      g_kernel.sched->ready_to_die = p->next;
    } else {
      prev->next = p->next;
    }
    p->next = NULL;
    return;
  }
}

void mp_remove_proc(proc_t* p) {
  proc_t* head;
  bool is_ready_queue = false;
  switch (p->status) {
    case PROC_STATUS_READY_TO_RUN:
    case PROC_STATUS_UNINITIALIZED:
      head = g_kernel.sched->first_ready_to_run;
      is_ready_queue = true;
      break;
    case PROC_STATUS_SLEEPING_TIMER:
      head = g_kernel.sched->sleeping;
      break;
    default:
      return;
  }

  if (p == head) {
    switch (p->status) {
      case PROC_STATUS_READY_TO_RUN:
      case PROC_STATUS_UNINITIALIZED:
        g_kernel.sched->first_ready_to_run = p->next;
        if (g_kernel.sched->last_ready_to_run == p) {
          g_kernel.sched->last_ready_to_run =
              g_kernel.sched->first_ready_to_run;
        }
        break;
      case PROC_STATUS_SLEEPING_TIMER:
        g_kernel.sched->sleeping = p->next;
        break;
      default:
        return;
    }
  } else {
    proc_t* last = head;
    proc_t* proc;
    for (proc = head; proc != p && proc != NULL; proc = proc->next) {
      last = proc;
    }
    if (proc != NULL) {
      last->next = proc->next;
      if (is_ready_queue && g_kernel.sched->last_ready_to_run == proc) {
        g_kernel.sched->last_ready_to_run = last;
      }
      proc->next = NULL;
    }
  }
}

void mp_set_last_ready_to_run(sched_state_t* mt, proc_t* first_ready_to_run) {
  if (first_ready_to_run->next == NULL) {
    mt->last_ready_to_run = first_ready_to_run;
    return;
  }
  mp_set_last_ready_to_run(mt, first_ready_to_run->next);
}
