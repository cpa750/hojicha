#ifndef TASK_H
#define TASK_H

#include <stdint.h>

typedef struct task_control_block task_control_block_t;
struct task_control_block {
  void* rsp;
  void* rsp0;
  void* cr3;
  task_control_block_t* next;
}

multitask_initialize();

#endif  // TASK_H

