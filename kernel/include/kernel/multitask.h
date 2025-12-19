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
  uint64_t elapsed;
  uint64_t switch_timestamp;
};

struct multitask_state;
typedef struct multitask_state multitask_state_t;

void multitask_state_dump(multitask_state_t* mt);

void multitask_initialize(void);

/*
 * Creates a new process with the given entry address.
 * The caller is expected to call multitask_free(task).
 */
process_block_t* multitask_new(proc_entry_t entry, void* cr3);

/*
 * Adds a proc to the scheduler's circular queue.
 */
void multitask_schedule_add_proc(process_block_t* process);

/*
 * Advances the scheduler.
 * Updates the current proc's elapsed counter, sets the next proc's switch
 * timestamp, and switches to the new proc.
 */
void multitask_schedule();
void multitask_switch(process_block_t* process);

#endif  // MULTITASK_H

