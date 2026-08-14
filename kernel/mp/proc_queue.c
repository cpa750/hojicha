#include "proc_queue.h"

#include <stddef.h>

void proc_queue_init(proc_queue_t* queue) {
  if (queue == NULL) { return; }
  queue->head = NULL;
  queue->tail = NULL;
}

bool proc_queue_empty(proc_queue_t* queue) {
  return queue == NULL || queue->head == NULL;
}

proc_t* proc_queue_head(proc_queue_t* queue) {
  if (queue == NULL) { return NULL; }
  return queue->head;
}

proc_t* proc_queue_tail(proc_queue_t* queue) {
  if (queue == NULL) { return NULL; }
  return queue->tail;
}

void proc_queue_push_head(proc_queue_t* queue, proc_t* proc) {
  if (queue == NULL || proc == NULL) { return; }

  proc_set_next(proc, queue->head);
  queue->head = proc;
  if (queue->tail == NULL) { queue->tail = proc; }
}

void proc_queue_push_tail(proc_queue_t* queue, proc_t* proc) {
  if (queue == NULL || proc == NULL) { return; }

  proc_set_next(proc, NULL);
  if (queue->head == NULL) {
    queue->head = proc;
    queue->tail = proc;
    return;
  }

  proc_set_next(queue->tail, proc);
  queue->tail = proc;
}

proc_t* proc_queue_pop_head(proc_queue_t* queue) {
  if (queue == NULL || queue->head == NULL) { return NULL; }

  proc_t* proc = queue->head;
  queue->head = proc_get_next(proc);
  if (queue->head == NULL) { queue->tail = NULL; }
  proc_set_next(proc, NULL);
  return proc;
}

void proc_queue_insert_after(proc_queue_t* queue, proc_t* after, proc_t* proc) {
  if (queue == NULL || after == NULL || proc == NULL) { return; }

  proc_set_next(proc, proc_get_next(after));
  proc_set_next(after, proc);
  if (queue->tail == after) { queue->tail = proc; }
}

proc_t* proc_queue_remove_after(proc_queue_t* queue, proc_t* prev) {
  if (queue == NULL) { return NULL; }

  if (prev == NULL) { return proc_queue_pop_head(queue); }

  proc_t* proc = proc_get_next(prev);
  if (proc == NULL) { return NULL; }

  proc_set_next(prev, proc_get_next(proc));
  if (queue->tail == proc) { queue->tail = prev; }
  proc_set_next(proc, NULL);
  return proc;
}

bool proc_queue_remove(proc_queue_t* queue, proc_t* proc) {
  if (queue == NULL || proc == NULL) { return false; }

  proc_t* prev = NULL;
  proc_t* curr = queue->head;
  while (curr != NULL) {
    if (curr == proc) {
      if (prev == NULL) {
        queue->head = proc_get_next(curr);
      } else {
        proc_set_next(prev, proc_get_next(curr));
      }
      if (queue->tail == curr) { queue->tail = prev; }
      proc_set_next(curr, NULL);
      return true;
    }

    prev = curr;
    curr = proc_get_next(curr);
  }

  return false;
}

bool proc_queue_find(proc_queue_t* queue,
                     proc_queue_pred_t pred,
                     void* ctx,
                     proc_t** prev_out,
                     proc_t** proc_out) {
  if (prev_out != NULL) { *prev_out = NULL; }
  if (proc_out != NULL) { *proc_out = NULL; }
  if (queue == NULL || pred == NULL) { return false; }

  proc_t* prev = NULL;
  for (proc_t* proc = queue->head; proc != NULL; proc = proc_get_next(proc)) {
    if (pred(proc, ctx)) {
      if (prev_out != NULL) { *prev_out = prev; }
      if (proc_out != NULL) { *proc_out = proc; }
      return true;
    }
    prev = proc;
  }

  return false;
}

proc_t* proc_queue_find_last(proc_queue_t* queue,
                             proc_queue_pred_t pred,
                             void* ctx) {
  if (queue == NULL || pred == NULL) { return NULL; }

  proc_t* last = NULL;
  for (proc_t* proc = queue->head; proc != NULL; proc = proc_get_next(proc)) {
    if (pred(proc, ctx)) { last = proc; }
  }
  return last;
}

proc_t* proc_queue_find_last_prefix(proc_queue_t* queue,
                                    proc_queue_pred_t pred,
                                    void* ctx) {
  if (queue == NULL || pred == NULL) { return NULL; }

  proc_t* last = NULL;
  for (proc_t* proc = queue->head; proc != NULL; proc = proc_get_next(proc)) {
    if (!pred(proc, ctx)) { break; }
    last = proc;
  }
  return last;
}

void proc_queue_for_each(proc_queue_t* queue,
                         proc_queue_each_t each,
                         void* ctx) {
  if (queue == NULL || each == NULL) { return; }

  for (proc_t* proc = queue->head; proc != NULL; proc = proc_get_next(proc)) {
    each(proc, ctx);
  }
}

void proc_queue_splice_prefix_tail(proc_queue_t* dst,
                                   proc_queue_t* src,
                                   proc_t* last) {
  if (dst == NULL || src == NULL || src->head == NULL || last == NULL) {
    return;
  }

  proc_t* first = src->head;
  proc_t* next = proc_get_next(last);
  src->head = next;
  if (src->tail == last) { src->tail = NULL; }
  proc_set_next(last, NULL);

  if (dst->head == NULL) {
    dst->head = first;
    dst->tail = last;
    return;
  }

  proc_set_next(dst->tail, first);
  dst->tail = last;
}
