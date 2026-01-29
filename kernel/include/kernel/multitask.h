#ifndef MULTITASK_H
#define MULTITASK_H

#include <haddr.h>
#include <stdint.h>

#define PROC_STATUS_PAUSED 0b00000100

/*
 * The entry point of the process. Must take no parameters and return void.
 */
typedef void (*proc_entry_t)(void);

/*
 * The process control block. This can be created via `multitask_new()`.
 */
typedef struct process_block process_block_t;
struct process_block;

struct multitask_state;
typedef struct multitask_state multitask_state_t;

void multitask_state_dump(multitask_state_t* mt);

void multitask_initialize(void);

/*
 * Adds a proc to the scheduler's queue.
 * The process will be added in a `READY_TO_RUN` state.
 */
void multitask_scheduler_add_proc(process_block_t* process);

/*
 * Creates a new process with the given entry address.
 * The caller is expected to call `multitask_free(task)`.
 */
process_block_t* multitask_new(proc_entry_t entry, void* cr3);

/*
 * Advances the scheduler if there is an available next process.
 * Updates the current proc's elapsed counter, sets the next proc's switch
 * timestamp, and switches to the new proc. If the current process is being
 * pre-empted, it is added to the ready to run queue.
 * The callee is required to lock before, and unlock after with the
 * `multitask_scheduler_lock()` and `multitask_scheduler_unlock()`
 * functions.
 */
void multitask_schedule(void);

void multitask_scheduler_lock(void);
void multitask_scheduler_unlock(void);

/*
 * Blocks the current process with the given `reason`.
 */
void multitask_block(uint8_t reason);

/*
 * Unblocks the given process. Only pre-empts if the process is the only one
 * running, otherwise the process is appended to the scheduling queue.
 */
void multitask_unblock(process_block_t* process);

/*
 * Sleeps the current process `s` seconds.
 */
void multitask_sleep(uint64_t s);

/*
 * Sleeps the current process `ns` nanoseconds.
 */
void multitask_sleep_ns(uint64_t ns);

void* multitask_process_block_get_cr3(process_block_t* p);

#endif  // MULTITASK_H

