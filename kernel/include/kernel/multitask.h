#ifndef MULTITASK_H
#define MULTITASK_H

#include <haddr.h>
#include <stdint.h>

typedef void (*proc_entry_t)(void);

typedef struct process_block process_block_t;
struct process_block {
  void* cr3;
  void* rsp;
  process_block_t* next;
  uint8_t status;
};

struct multitask_state;
typedef struct multitask_state multitask_state_t;

void multitask_state_dump(multitask_state_t* mt);

void multitask_initialize(void);
process_block_t* multitask_new(proc_entry_t entry, void* cr3);
void multitask_schedule_add_proc(process_block_t* process);
void multitask_schedule();
void multitask_switch(process_block_t* process);

#endif  // MULTITASK_H

