#ifndef HOJICHA_MP_PROC_QUEUE_H
#define HOJICHA_MP_PROC_QUEUE_H

#include <mp/proc.h>
#include <stdbool.h>

typedef struct proc_queue {
  proc_t* head;
  proc_t* tail;
} proc_queue_t;

typedef bool (*proc_queue_pred_t)(proc_t* proc, void* ctx);
typedef void (*proc_queue_each_t)(proc_t* proc, void* ctx);

void proc_queue_init(proc_queue_t* queue);
bool proc_queue_empty(proc_queue_t* queue);
proc_t* proc_queue_head(proc_queue_t* queue);
proc_t* proc_queue_tail(proc_queue_t* queue);

void proc_queue_push_head(proc_queue_t* queue, proc_t* proc);
void proc_queue_push_tail(proc_queue_t* queue, proc_t* proc);
proc_t* proc_queue_pop_head(proc_queue_t* queue);

void proc_queue_insert_after(proc_queue_t* queue, proc_t* after, proc_t* proc);
proc_t* proc_queue_remove_after(proc_queue_t* queue, proc_t* prev);
bool proc_queue_remove(proc_queue_t* queue, proc_t* proc);

bool proc_queue_find(proc_queue_t* queue,
                     proc_queue_pred_t match,
                     void* ctx,
                     proc_t** prev_out,
                     proc_t** proc_out);
proc_t* proc_queue_find_last(proc_queue_t* queue,
                             proc_queue_pred_t match,
                             void* ctx);
proc_t* proc_queue_find_last_prefix(proc_queue_t* queue,
                                    proc_queue_pred_t match,
                                    void* ctx);
void proc_queue_for_each(proc_queue_t* queue,
                         proc_queue_each_t each,
                         void* ctx);

void proc_queue_splice_prefix_tail(proc_queue_t* dst,
                                   proc_queue_t* src,
                                   proc_t* last);

#endif  // HOJICHA_MP_PROC_QUEUE_H
