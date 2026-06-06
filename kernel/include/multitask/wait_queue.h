#ifndef HOJICHA_MULTITASK_WAIT_QUEUE_H
#define HOJICHA_MULTITASK_WAIT_QUEUE_H

#include <stdbool.h>
#include <multitask/scheduler.h>

typedef struct wait_queue {
  process_block_t* head;
  process_block_t* tail;
} wait_queue_t;

void wait_queue_init(wait_queue_t* q);
bool wait_queue_empty(wait_queue_t* q);
void wait_queue_sleep(wait_queue_t* q);
process_block_t* wait_queue_wake_one(wait_queue_t* q);
void wait_queue_wake_all(wait_queue_t* q);

#endif  // HOJICHA_MULTITASK_WAIT_QUEUE_H
