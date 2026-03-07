#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <multitask/semaphore.h>
#include <stdlib.h>

struct semaphore {
  uint8_t limit;
  uint8_t current_locks;
  process_block_t* queue_head;
  process_block_t* queue_tail;
};

semaphore_t* semaphore_create(uint8_t limit) {
  semaphore_t* s = (semaphore_t*)malloc(sizeof(semaphore_t));

  if (s != NULL) {
    s->limit = limit;
    s->current_locks = 0;
    s->queue_head = NULL;
    s->queue_tail = NULL;
  }

  return s;
}

void semaphore_destroy(semaphore_t* s) { free(s); }

void semaphore_lock(semaphore_t* s) {
  sched_postpone();
  if (s->current_locks < s->limit) {
    s->current_locks++;
  } else {
    sched_pb_set_next(g_kernel.current_process, NULL);
    if (s->queue_head == NULL) {
      s->queue_head = g_kernel.current_process;
    } else {
      sched_pb_set_next(s->queue_tail, g_kernel.current_process);
    }
    s->queue_tail = g_kernel.current_process;
    sched_current_block(PROC_STATUS_SEMAPHORE);
  }
  sched_resume();
}

bool semaphore_try_lock(semaphore_t* s) {
  if (s->current_locks < s->limit) {
    semaphore_lock(s);
    return true;
  }

  return false;
}

void semaphore_unlock(semaphore_t* s) {
  sched_postpone();
  if (s->queue_head != NULL) {
    process_block_t* proc = s->queue_head;
    s->queue_head = sched_pb_get_next(s->queue_head);
    sched_proc_unblock(proc);
  } else {
    s->current_locks--;
  }

  sched_resume();
}

