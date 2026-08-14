#ifndef HOJICHA_MP_UTILS_H
#define HOJICHA_MP_UTILS_H

#include <mp/proc.h>
#include <mp/scheduler.h>
#include <stdbool.h>
#include <stdint.h>

#include "proc_queue.h"

struct sched_state {
  proc_queue_t ready_to_run;
  proc_queue_t ready_to_die;
  proc_queue_t sleeping;

  uint64_t total_processes_added;
  uint64_t process_count;

  uint64_t idle_switch_timestamp;
  uint64_t quantum_remaining;
  uint64_t tick_interval_ns;
  uint64_t time_elapsed;
  uint64_t time_idle;

  uint64_t irq_lock_count;
  uint64_t switch_lock_count;
  bool switch_lock_flag;

  uint64_t kernel_pid;
};

proc_queue_t* mp_ready_queue(void);
proc_queue_t* mp_ready_to_die_queue(void);
proc_queue_t* mp_sleeping_queue(void);
void mp_block_proc(proc_t* p, uint8_t reason);
void mp_enqueue_ready(proc_t* process);
void mp_ready_to_die_remove(proc_t* target);
void mp_remove_proc(proc_t* p);

#endif  // HOJICHA_MP_UTILS_H
