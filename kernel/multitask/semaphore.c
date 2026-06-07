#include <multitask/scheduler.h>
#include <multitask/semaphore.h>
#include <multitask/wait_queue.h>
#include <stdlib.h>

struct semaphore {
  uint8_t limit;
  uint8_t current_locks;
  wait_queue_t waiters;
};

semaphore_t* semaphore_create(uint8_t limit) {
  semaphore_t* s = (semaphore_t*)malloc(sizeof(semaphore_t));

  if (s != NULL) {
    s->limit = limit;
    s->current_locks = 0;
    wait_queue_init(&s->waiters);
  }

  return s;
}

void semaphore_destroy(semaphore_t* s) { free(s); }

void semaphore_lock(semaphore_t* s) {
  sched_postpone();
  if (s->current_locks < s->limit) {
    s->current_locks++;
  } else {
    wait_queue_sleep(&s->waiters);
  }
  sched_resume();
}

bool semaphore_try_lock(semaphore_t* s) {
  sched_postpone();
  if (s->current_locks < s->limit) {
    semaphore_lock(s);
    sched_resume();
    return true;
  }

  sched_resume();
  return false;
}

void semaphore_unlock(semaphore_t* s) {
  sched_postpone();
  if (!wait_queue_empty(&s->waiters)) {
    wait_queue_wake_one(&s->waiters);
  } else {
    s->current_locks--;
  }

  sched_resume();
}
