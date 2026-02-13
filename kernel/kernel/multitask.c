#include <drivers/pit.h>
#include <haddr.h>
#include <hlog.h>
#include <kernel/kernel_state.h>
#include <kernel/multitask.h>
#include <memory/vmm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define PROC_STATUS_UNINITIALIZED 0b11111111
#define PROC_STATUS_RUNNING       0b00000001
#define PROC_STATUS_READY_TO_RUN  0b00000010
#define PROC_STATUS_READY_TO_DIE  0b00000100
#define STACK_SIZE                16384
#define QUANTUM_LENGTH            5000000  // 5 ms

struct multitask_state {
  process_block_t* first_ready_to_run;
  process_block_t* last_ready_to_run;
  process_block_t* ready_to_die;
  process_block_t* sleeping;

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
};

typedef struct process_block process_block_t;
struct process_block {
  // Begin asm-mapped fields
  void* cr3;
  void* rsp;
  uint8_t status;
  proc_entry_t entry;
  // End asm-mapped fields

  hlogger_t* logger;
  char* name;
  uint64_t pid;

  process_block_t* next;
  void* stack_end;

  uint64_t elapsed;
  uint64_t sleep_until;
  uint64_t switch_timestamp;
};

process_block_t* multitask_pb_get_next(process_block_t* p) { return p->next; }
void multitask_pb_set_next(process_block_t* p, process_block_t* next) {
  p->next = next;
}
hlogger_t* multitask_pb_get_logger(process_block_t* p) { return p->logger; }
char* multitask_pb_get_name(process_block_t* p) { return p->name; }
uint64_t multitask_pb_get_pid(process_block_t* p) { return p->pid; }

static multitask_state_t mt = {0};
static pit_callback_t pit_callback = {0};

extern void switch_to(process_block_t* process);

void multitask_switch(process_block_t* process);

void block_process(process_block_t* p, uint8_t reason);
process_block_t* find_last_sleep_timestamp_less_than_equal(
    process_block_t* process,
    uint64_t timestamp);
void handle_timer(uint64_t timestamp);
void insert_process_after(process_block_t* process, process_block_t* after);
void mark_proc_range(process_block_t* start,
                     process_block_t* end,
                     uint8_t status);
void proc_entry_wrapper(process_block_t* p);
void set_last_ready_to_run(multitask_state_t* mt,
                           process_block_t* first_ready_to_run);
void remove_proc(process_block_t* p);
void sleep_proc_until(process_block_t* process, uint64_t timestamp);
void terminator(void);
void wake_procs_before_timestamp(uint64_t timestamp);

void multitask_initialize(void) {
  process_block_t* kernel_process =
      (process_block_t*)malloc(sizeof(process_block_t));
  haddr_t cr3;
  haddr_t rsp;
  asm volatile("\t movq %%cr3,%0" : "=r"(cr3));
  asm volatile("\t movq %%rsp,%0" : "=r"(rsp));
  kernel_process->cr3 = (void*)cr3;
  kernel_process->rsp = (void*)rsp;
  kernel_process->next = NULL;
  kernel_process->status = PROC_STATUS_RUNNING;
  kernel_process->name = "hojicha";
  kernel_process->logger = hlog_new(DEFAULT_HLOG_LEVEL, DEFAULT_HLOG_BUFSIZE);
  kernel_process->pid = UINT64_MAX;

  mt.first_ready_to_run = NULL;
  mt.last_ready_to_run = NULL;
  mt.ready_to_die = NULL;

  // Initially set to 1 as we don't directly add kernel proc
  mt.total_processes_added = 1;
  mt.process_count = 1;

  mt.tick_interval_ns = pit_state_get_tick_interval_ns(g_kernel.pit);
  g_kernel.mt = &mt;
  g_kernel.current_process = kernel_process;
  pit_callback.callback_func = handle_timer;
  pit_callback.next = NULL;
  pit_register_callback(&pit_callback);

  process_block_t* terminator_task =
      multitask_proc_new("kterminator", terminator, kernel_process->cr3);
  multitask_scheduler_add_proc(terminator_task);
}

process_block_t* multitask_proc_new(char* name, proc_entry_t entry, void* cr3) {
  ++(g_kernel.mt->total_processes_added);
  ++(g_kernel.mt->process_count);
  process_block_t* new_proc = (process_block_t*)malloc(sizeof(process_block_t));
  new_proc->cr3 = cr3;
  new_proc->entry = entry;
  uint8_t* stack_end = (uint8_t*)malloc(STACK_SIZE);
  new_proc->stack_end = stack_end;
  haddr_t stack_base = (haddr_t)(stack_end + STACK_SIZE);
  stack_base &= ~0xFULL;
  stack_base -= 8;
  *(haddr_t*)stack_base = (haddr_t)proc_entry_wrapper;
  stack_base -= 8 * 6;
  *(haddr_t*)stack_base = (haddr_t)new_proc;

  new_proc->pid = g_kernel.mt->total_processes_added;
  if (name != NULL) {
    new_proc->name = name;
  } else {
    itoa(new_proc->pid, new_proc->name, 10);
  }
  new_proc->logger = hlog_new(DEFAULT_HLOG_LEVEL, DEFAULT_HLOG_BUFSIZE);

  // Because we pop 15 registers from the newly allocated stack
  // in switch_to()
  stack_base -= 8 * 9;
  new_proc->rsp = (void*)stack_base;
  new_proc->status = PROC_STATUS_UNINITIALIZED;
  return new_proc;
}

void multitask_scheduler_add_proc(process_block_t* process) {
  multitask_scheduler_postpone();
  process->next = NULL;
  if (g_kernel.mt->first_ready_to_run == NULL) {
    g_kernel.mt->first_ready_to_run = process;
    g_kernel.mt->last_ready_to_run = process;
    multitask_scheduler_resume();
    return;
  }

  if (g_kernel.mt->last_ready_to_run == NULL) {
    set_last_ready_to_run(g_kernel.mt, g_kernel.mt->first_ready_to_run);
  }
  g_kernel.mt->last_ready_to_run->next = process;
  g_kernel.mt->last_ready_to_run = process;
  multitask_scheduler_resume();
}

void multitask_schedule(void) {
  // TODO: refactor this mess of a function
  if (g_kernel.mt->switch_lock_count > 0) {
    g_kernel.mt->switch_lock_flag = true;
    return;
  }

  g_kernel.mt->time_elapsed = pit_get_ns_elapsed_since_init(g_kernel.pit);
  if (g_kernel.current_process != NULL) {
    g_kernel.current_process->elapsed +=
        g_kernel.mt->time_elapsed - g_kernel.current_process->switch_timestamp;
  } else {
    g_kernel.mt->time_idle +=
        g_kernel.mt->time_elapsed - g_kernel.mt->idle_switch_timestamp;
  }

  if (g_kernel.mt->first_ready_to_run != NULL) {
    if (g_kernel.current_process != NULL &&
        g_kernel.current_process->status == PROC_STATUS_RUNNING) {
      g_kernel.current_process->status = PROC_STATUS_READY_TO_RUN;

      // We only want to place the old process in the ready-to-run queue if
      // it's being pre-empted. Otherwise, (for example, sleeping) it belongs
      // in a difference queue handled elsewhere.
      if (g_kernel.mt->last_ready_to_run == NULL) {
        g_kernel.mt->last_ready_to_run = g_kernel.mt->first_ready_to_run;
      }
      g_kernel.mt->last_ready_to_run->next = g_kernel.current_process;
      g_kernel.mt->last_ready_to_run = g_kernel.mt->last_ready_to_run->next;
      if (g_kernel.mt->first_ready_to_run != g_kernel.mt->last_ready_to_run) {
        g_kernel.mt->last_ready_to_run->next = NULL;
      }
    }

    g_kernel.mt->quantum_remaining = QUANTUM_LENGTH;
    process_block_t* next = g_kernel.mt->first_ready_to_run;
    next->switch_timestamp = g_kernel.mt->time_elapsed;
    g_kernel.mt->first_ready_to_run = g_kernel.mt->first_ready_to_run->next;
    if (next == g_kernel.mt->last_ready_to_run) {
      g_kernel.mt->last_ready_to_run = NULL;
    }

    // TODO: what happens if we sleep the only available process?
    // g_kernel.current_process and its status is updated inside switch_to()
    multitask_switch(next);
  } else if (g_kernel.current_process->status != PROC_STATUS_RUNNING) {
    process_block_t* proc = g_kernel.current_process;
    g_kernel.current_process = NULL;
    g_kernel.mt->idle_switch_timestamp = g_kernel.mt->time_elapsed;

    do {
      asm volatile("sti");
      asm volatile("hlt");
      asm volatile("cli");
    } while (g_kernel.mt->first_ready_to_run == NULL);

    g_kernel.mt->quantum_remaining = 0;
    g_kernel.current_process = proc;
    proc->switch_timestamp = g_kernel.mt->time_elapsed;
    proc = g_kernel.mt->first_ready_to_run;
    g_kernel.mt->first_ready_to_run = g_kernel.mt->first_ready_to_run->next;
    if (proc == g_kernel.mt->last_ready_to_run) {
      g_kernel.mt->last_ready_to_run = NULL;
    }
    multitask_switch(proc);
  }
}

void multitask_switch(process_block_t* process) {
  asm volatile("cli");
  switch_to(process);
  asm volatile("sti");
}

void multitask_scheduler_lock(void) {
  // TODO: this will need more fleshing out for multi-core support
  asm volatile("cli");
  g_kernel.mt->irq_lock_count++;
}

void multitask_scheduler_unlock(void) {
  if (g_kernel.mt->irq_lock_count > 0) { g_kernel.mt->irq_lock_count--; }
  if (g_kernel.mt->irq_lock_count == 0) { asm volatile("sti"); }
}

void multitask_scheduler_postpone(void) {
  // TODO: this will need more fleshing out for multi-core support
  asm volatile("cli");
  g_kernel.mt->irq_lock_count++;
  g_kernel.mt->switch_lock_count++;
}

void multitask_scheduler_resume(void) {
  if (g_kernel.mt->switch_lock_count > 0) { g_kernel.mt->switch_lock_count--; }
  if (g_kernel.mt->switch_lock_count == 0 && g_kernel.mt->switch_lock_flag) {
    g_kernel.mt->switch_lock_flag = false;
    multitask_schedule();
  }
  if (g_kernel.mt->irq_lock_count > 0) { g_kernel.mt->irq_lock_count--; }
  if (g_kernel.mt->irq_lock_count == 0) { asm volatile("sti"); }
}

void multitask_block(uint8_t reason) {
  block_process(g_kernel.current_process, reason);
}

void multitask_unblock(process_block_t* process) {
  multitask_scheduler_lock();

  if (g_kernel.mt->first_ready_to_run != NULL) {
    process->status = PROC_STATUS_READY_TO_RUN;
    g_kernel.mt->last_ready_to_run->next = process;
    g_kernel.mt->last_ready_to_run = process;
  } else {
    multitask_switch(process);
  }

  multitask_scheduler_unlock();
}

void multitask_sleep(uint64_t s) { multitask_sleep_ns(s * 1000000000ULL); }

void multitask_sleep_ns(uint64_t ns) {
  sleep_proc_until(g_kernel.current_process,
                   pit_get_ns_elapsed_since_init(g_kernel.pit) + ns);
}

void multitask_proc_terminate(process_block_t* p) {
  multitask_scheduler_postpone();

  multitask_scheduler_lock();
  if (p != g_kernel.current_process) { remove_proc(p); }
  p->next = g_kernel.mt->ready_to_die;
  g_kernel.mt->ready_to_die = p;
  multitask_scheduler_unlock();

  block_process(p, PROC_STATUS_READY_TO_DIE);
  multitask_scheduler_resume();
}

void* multitask_process_block_get_cr3(process_block_t* p) { return p->cr3; }

void block_process(process_block_t* p, uint8_t reason) {
  multitask_scheduler_lock();
  p->status = reason;
  multitask_schedule();
  multitask_scheduler_unlock();
}

/*
 * Finds the last process needing to be woken.
 */
process_block_t* find_last_sleep_timestamp_less_than_equal(
    process_block_t* process,
    uint64_t timestamp) {
  if (process->sleep_until > timestamp) { return NULL; }
  process_block_t* ret = NULL;
  for (process_block_t* curr = process; curr != NULL; curr = curr->next) {
    if (curr->sleep_until <= timestamp) {
      ret = curr;
    } else {
      break;
    }
  }
  return ret;
}

void handle_timer(uint64_t timestamp) {
  // TODO: figure out why calls to printf() that are preempted break
  // framebuffer line scrolling
  multitask_scheduler_postpone();
  wake_procs_before_timestamp(timestamp);
  if (g_kernel.mt->quantum_remaining != 0) {
    if (g_kernel.mt->quantum_remaining <= g_kernel.mt->tick_interval_ns) {
      multitask_scheduler_lock();
      multitask_schedule();
      multitask_scheduler_unlock();
    } else {
      g_kernel.mt->quantum_remaining -= g_kernel.mt->tick_interval_ns;
    }
  }
  multitask_scheduler_resume();
}

void insert_process_after(process_block_t* process, process_block_t* after) {
  if (after == NULL) { return; }

  process->next = after->next;
  after->next = process;
}

void proc_entry_wrapper(process_block_t* p) {
  if (p == NULL) { return; }
  if (p->status == PROC_STATUS_UNINITIALIZED) {
    p->status = PROC_STATUS_RUNNING;
    multitask_scheduler_unlock();
  }
  p->entry();
  multitask_proc_terminate(p);
}

void remove_proc(process_block_t* p) {
  // TODO: tidy this up
  process_block_t* head;
  switch (p->status) {
    case PROC_STATUS_READY_TO_RUN:
    case PROC_STATUS_UNINITIALIZED:
      head = g_kernel.mt->first_ready_to_run;
      break;
    case PROC_STATUS_PAUSED:
      head = g_kernel.mt->sleeping;
      break;
    default:
      return;
  }

  if (p == head) {
    switch (p->status) {
      case PROC_STATUS_READY_TO_RUN:
      case PROC_STATUS_UNINITIALIZED:
        g_kernel.mt->first_ready_to_run = p->next;
        break;
      case PROC_STATUS_PAUSED:
        g_kernel.mt->sleeping = p->next;
        break;
      default:
        return;
    }
  } else {
    process_block_t* last = head;
    process_block_t* proc;
    for (proc = head; proc != p && proc != NULL; proc = proc->next) {
      last = proc;
    }
    if (proc != NULL) {
      last->next = proc->next;
      proc->next = NULL;
    }
  }
}

void set_last_ready_to_run(multitask_state_t* mt,
                           process_block_t* first_ready_to_run) {
  if (first_ready_to_run->next == NULL) {
    mt->last_ready_to_run = first_ready_to_run;
    return;
  }
  set_last_ready_to_run(mt, first_ready_to_run->next);
}

void sleep_proc_until(process_block_t* process, uint64_t timestamp) {
  multitask_scheduler_postpone();
  process->sleep_until = timestamp;
  process->status = PROC_STATUS_PAUSED;

  // TODO rewrite this to use `multitask_block()`

  if (g_kernel.mt->sleeping == NULL) {
    process->next = NULL;
    g_kernel.mt->sleeping = process;
    multitask_scheduler_lock();
    multitask_schedule();
    multitask_scheduler_unlock();
    multitask_scheduler_resume();
    return;
  }

  process_block_t* last = find_last_sleep_timestamp_less_than_equal(
      g_kernel.mt->sleeping, timestamp);
  if (last == NULL) {
    process->next = g_kernel.mt->sleeping;
    g_kernel.mt->sleeping = process;
  }
  insert_process_after(process, last);

  multitask_scheduler_lock();
  multitask_schedule();
  multitask_scheduler_unlock();
  multitask_scheduler_resume();
}

/*
 * The task to terminate all tasks.
 */
void terminator(void) {
  while (true) {
    if (g_kernel.mt->ready_to_die == NULL) {
      // TODO: improve this so it immediately yields with `multitask_block()`
      multitask_scheduler_lock();
      multitask_schedule();
      multitask_scheduler_unlock();
    } else {
      multitask_scheduler_postpone();
      for (process_block_t* p = g_kernel.mt->ready_to_die; p != NULL;
           p = p->next) {
        // TODO: we ideally need to hand the logs committing off to a
        // dedicated task
        hlog_free_logger(p->logger);
        free(p->stack_end);
        free(p);
      }
      g_kernel.mt->ready_to_die = NULL;
      multitask_scheduler_resume();
    }
  }
}

void wake_procs_before_timestamp(uint64_t timestamp) {
  if (g_kernel.mt->sleeping == NULL ||
      g_kernel.mt->sleeping->sleep_until > timestamp) {
    return;
  }

  process_block_t* last_to_wake = NULL;
  for (process_block_t* curr = g_kernel.mt->sleeping; curr != NULL;
       curr = curr->next) {
    if (curr->sleep_until <= timestamp) {
      curr->status = PROC_STATUS_READY_TO_RUN;
      last_to_wake = curr;
    } else {
      break;
    }
  }

  if (g_kernel.mt->first_ready_to_run == NULL) {
    g_kernel.mt->first_ready_to_run = g_kernel.mt->sleeping;
    g_kernel.mt->last_ready_to_run = last_to_wake;
  } else {
    g_kernel.mt->last_ready_to_run->next = g_kernel.mt->sleeping;
    g_kernel.mt->last_ready_to_run = last_to_wake;
  }

  g_kernel.mt->sleeping = last_to_wake->next;
  g_kernel.mt->last_ready_to_run->next = NULL;
}
