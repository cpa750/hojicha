#include <kernel/g_kernel.h>
#include <multitask/wait_queue.h>
#include <stddef.h>

static void enqueue_current(wait_queue_t* q);

void wait_queue_init(wait_queue_t* q) {
  if (q == NULL) { return; }
  q->head = NULL;
  q->tail = NULL;
}

bool wait_queue_empty(wait_queue_t* q) {
  return q == NULL || q->head == NULL;
}

void wait_queue_sleep(wait_queue_t* q) {
  if (q == NULL || g_kernel.current_process == NULL) { return; }

  sched_postpone();
  enqueue_current(q);
  sched_current_block(PROC_STATUS_BLOCKED);
  sched_resume();
}

void wait_queue_sleep_postponed(wait_queue_t* q) {
  if (q == NULL || g_kernel.current_process == NULL) { return; }

  enqueue_current(q);
  sched_current_block(PROC_STATUS_BLOCKED);
  sched_resume();
  sched_postpone();
}

process_block_t* wait_queue_wake_one(wait_queue_t* q) {
  if (q == NULL) { return NULL; }

  sched_postpone();
  process_block_t* proc = q->head;
  if (proc != NULL) {
    q->head = sched_pb_get_next(proc);
    if (q->head == NULL) { q->tail = NULL; }
    sched_pb_set_next(proc, NULL);
    sched_proc_unblock(proc);
  }
  sched_resume();
  return proc;
}

void wait_queue_wake_all(wait_queue_t* q) {
  if (q == NULL) { return; }

  sched_postpone();
  while (q->head != NULL) {
    process_block_t* proc = q->head;
    q->head = sched_pb_get_next(proc);
    sched_pb_set_next(proc, NULL);
    sched_proc_unblock(proc);
  }
  q->tail = NULL;
  sched_resume();
}

static void enqueue_current(wait_queue_t* q) {
  if (q == NULL || g_kernel.current_process == NULL) { return; }

  sched_pb_set_next(g_kernel.current_process, NULL);
  if (q->head == NULL) {
    q->head = g_kernel.current_process;
  } else {
    sched_pb_set_next(q->tail, g_kernel.current_process);
  }
  q->tail = g_kernel.current_process;
}
