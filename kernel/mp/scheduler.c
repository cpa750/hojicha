#include <cpu/fpu.h>
#include <drivers/pit.h>
#include <errno.h>
#include <fs/vfs.h>
#include <haddr.h>
#include <hlog.h>
#include <kernel/g_kernel.h>
#include <memory/slab.h>
#include <mp/proc.h>
#include <mp/scheduler.h>
#include <mp/sleep.h>
#include <mp/wait_queue.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp_utils.h"

#define QUANTUM_LENGTH 50000  // 0.5 ms

uint64_t sched_state_get_kernel_pid(sched_state_t* mt) {
  return mt->kernel_pid;
}

proc_t* sched_state_get_ready_head(sched_state_t* mt) {
  return proc_queue_head(&mt->ready_to_run);
}

proc_t* sched_state_get_sleeping_head(sched_state_t* mt) {
  return proc_queue_head(&mt->sleeping);
}

proc_t* sched_state_get_ready_to_die_head(sched_state_t* mt) {
  return proc_queue_head(&mt->ready_to_die);
}

static sched_state_t mt = {0};
static pit_callback_t pit_callback = {0};

extern void switch_to(proc_t* process, bool is_ctx_switch);

void multitask_switch(proc_t* process);
void handle_timer(uint64_t timestamp);
void terminator(void);

void sched_initialize(void) {
  proc_t* kernel_process =
      (proc_t*)slab_calloc(sizeof(proc_t));
  haddr_t cr3;
  haddr_t rsp;
  asm volatile("\t movq %%cr3,%0" : "=r"(cr3));
  asm volatile("\t movq %%rsp,%0" : "=r"(rsp));
  kernel_process->cr3 = (void*)cr3;
  kernel_process->rsp = (void*)rsp;
  kernel_process->rsp0 = (void*)rsp;
  kernel_process->next = NULL;
  kernel_process->status = PROC_STATUS_RUNNING;
  kernel_process->is_kernel_proc = true;
  kernel_process->pid = 0;
  kernel_process->name = proc_name_new("hojicha", strlen("hojicha"));
  kernel_process->logger = hlog_new(DEFAULT_HLOG_LEVEL, DEFAULT_HLOG_BUFSIZE);
  kernel_process->mem = process_mem_new(g_kernel.vmm);
  kernel_process->fds =
      (vfs_file_t**)slab_calloc(sizeof(vfs_file_t*) * MAX_FDS);
  kernel_process->children =
      (proc_t**)slab_calloc(sizeof(proc_t*) * MAX_CHILDREN);
  wait_queue_init(&kernel_process->child_waiters);
  wait_queue_init(&kernel_process->exit_waiters);
  kernel_process->fpu = g_kernel.has_fpu ? fpu_new() : NULL;
  if (kernel_process->name == NULL || kernel_process->logger == NULL ||
      kernel_process->mem == NULL || kernel_process->fds == NULL ||
      kernel_process->children == NULL ||
      (g_kernel.has_fpu && kernel_process->fpu == NULL)) {
    slab_free(kernel_process->name);
    if (kernel_process->logger != NULL) {
      hlog_free_logger(kernel_process->logger);
    }
    slab_free(kernel_process->fds);
    slab_free(kernel_process->children);
    fpu_free(kernel_process->fpu);
    process_mem_free(kernel_process->mem);
    slab_free(kernel_process);
    abort();
  }
  mt.kernel_pid = kernel_process->pid;

  proc_queue_init(&mt.ready_to_run);
  proc_queue_init(&mt.ready_to_die);
  proc_queue_init(&mt.sleeping);

  // Initially set to 1 as we don't directly add kernel proc
  mt.total_processes_added = 1;
  mt.process_count = 1;

  mt.tick_interval_ns = pit_state_get_tick_interval_ns(g_kernel.pit);
  g_kernel.sched = &mt;
  g_kernel.current_process = kernel_process;
  pit_callback.callback_func = handle_timer;
  pit_callback.next = NULL;
  pit_register_callback(&pit_callback);

  proc_t* terminator_task =
      proc_new_kernel("kterminator", terminator, kernel_process->cr3);
  sched_add_proc(terminator_task);
}

void sched_add_proc(proc_t* process) {
  sched_postpone();
  mp_enqueue_ready(process);
  sched_resume();
}

void schedule_advance(void) {
  // TODO: refactor this mess of a function
  if (g_kernel.sched->switch_lock_count > 0) {
    g_kernel.sched->switch_lock_flag = true;
    return;
  }

  g_kernel.sched->time_elapsed = pit_get_ns_elapsed_since_init(g_kernel.pit);
  if (g_kernel.current_process != NULL) {
    g_kernel.current_process->elapsed +=
        g_kernel.sched->time_elapsed -
        g_kernel.current_process->switch_timestamp;
  } else {
    g_kernel.sched->time_idle +=
        g_kernel.sched->time_elapsed - g_kernel.sched->idle_switch_timestamp;
  }

  if (!proc_queue_empty(mp_ready_queue())) {
    if (g_kernel.current_process != NULL &&
        g_kernel.current_process->status == PROC_STATUS_RUNNING) {
      g_kernel.current_process->status = PROC_STATUS_READY_TO_RUN;

      // We only want to place the old process in the ready-to-run queue if
      // it's being pre-empted. Otherwise, (for example, sleeping) it belongs
      // in a difference queue handled elsewhere.
      proc_queue_push_tail(mp_ready_queue(), g_kernel.current_process);
    }

    g_kernel.sched->quantum_remaining = QUANTUM_LENGTH;
    proc_t* next = proc_queue_pop_head(mp_ready_queue());
    next->switch_timestamp = g_kernel.sched->time_elapsed;

    // TODO: what happens if we sleep the only available process?
    // g_kernel.current_process and its status is updated inside switch_to()
    multitask_switch(next);
  } else if (g_kernel.current_process->status != PROC_STATUS_RUNNING) {
    proc_t* proc = g_kernel.current_process;
    g_kernel.current_process = NULL;
    g_kernel.sched->idle_switch_timestamp = g_kernel.sched->time_elapsed;

    do {
      asm volatile("sti");
      asm volatile("hlt");
      asm volatile("cli");
    } while (proc_queue_empty(mp_ready_queue()));

    g_kernel.sched->quantum_remaining = 0;
    g_kernel.current_process = proc;
    proc->switch_timestamp = g_kernel.sched->time_elapsed;
    proc = proc_queue_pop_head(mp_ready_queue());
    multitask_switch(proc);
  }
}

void sched_yield(void) {
  sched_lock();
  schedule_advance();
  sched_unlock();
}

void multitask_switch(proc_t* process) {
  asm volatile("cli");
  uint64_t cs = 0;
  asm volatile("\t movq %%cs,%0" : "=r"(cs));
  uint8_t cpl = cs & 0b11;
  bool is_ctx_switch = (!cpl && !process->is_kernel_proc) ||
                       (cpl == 3 && process->is_kernel_proc);
  if (g_kernel.has_fpu) {
    if (g_kernel.current_process != NULL) {
      fpu_save(g_kernel.current_process->fpu);
    }
    fpu_restore(process->fpu);
  }
  switch_to(process, is_ctx_switch);
  asm volatile("sti");
}

void sched_lock(void) {
  // TODO: this will need more fleshing out for multi-core support
  asm volatile("cli");
  g_kernel.sched->irq_lock_count++;
}

void sched_unlock(void) {
  if (g_kernel.sched->irq_lock_count > 0) { g_kernel.sched->irq_lock_count--; }
  if (g_kernel.sched->irq_lock_count == 0) { asm volatile("sti"); }
}

void sched_postpone(void) {
  // TODO: this will need more fleshing out for multi-core support
  asm volatile("cli");
  g_kernel.sched->irq_lock_count++;
  g_kernel.sched->switch_lock_count++;
}

void sched_resume(void) {
  if (g_kernel.sched->switch_lock_count > 0) {
    g_kernel.sched->switch_lock_count--;
  }
  if (g_kernel.sched->switch_lock_count == 0 &&
      g_kernel.sched->switch_lock_flag) {
    g_kernel.sched->switch_lock_flag = false;
    schedule_advance();
  }
  if (g_kernel.sched->irq_lock_count > 0) { g_kernel.sched->irq_lock_count--; }
  if (g_kernel.sched->irq_lock_count == 0) { asm volatile("sti"); }
}

void handle_timer(uint64_t timestamp) {
  // TODO: figure out why calls to printf() that are preempted break
  // framebuffer line scrolling
  sched_postpone();
  wake_procs_before_timestamp(timestamp);
  if (g_kernel.sched->quantum_remaining != 0) {
    if (g_kernel.sched->quantum_remaining <= g_kernel.sched->tick_interval_ns) {
      sched_lock();
      schedule_advance();
      sched_unlock();
    } else {
      g_kernel.sched->quantum_remaining -= g_kernel.sched->tick_interval_ns;
    }
  }
  sched_resume();
}

/*
 * The task to terminate all tasks.
 */
void terminator(void) {
  while (true) {
    if (proc_queue_empty(mp_ready_to_die_queue())) {
      // TODO: improve this so it blocks until a proc is ready to reap.
      sched_lock();
      schedule_advance();
      sched_unlock();
    } else {
      sched_postpone();
      proc_queue_t waiting_for_parent;
      proc_queue_init(&waiting_for_parent);
      bool freed_process = false;
      for (proc_t* p = proc_queue_pop_head(mp_ready_to_die_queue()); p != NULL;
           p = proc_queue_pop_head(mp_ready_to_die_queue())) {
        if (p->parent != NULL) {
          proc_queue_push_head(&waiting_for_parent, p);
        } else {
          // TODO: we ideally need to hand the logs committing off to a
          // dedicated task
          proc_free(p);
          freed_process = true;
        }
      }
      while (!proc_queue_empty(&waiting_for_parent)) {
        proc_queue_push_tail(mp_ready_to_die_queue(),
                             proc_queue_pop_head(&waiting_for_parent));
      }
      sched_resume();
      if (!freed_process) { sched_yield(); }
    }
  }
}
