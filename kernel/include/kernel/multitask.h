#ifndef MULTITASK_H
#define MULTITASK_H

#include <haddr.h>
#include <hlog.h>
#include <stdint.h>

#define PROC_STATUS_PAUSED    0b00000100
#define PROC_STATUS_SEMAPHORE 0b00001000

/*
 * The entry point of the process. Must take no parameters and return void.
 */
typedef void (*proc_entry_t)(void);

/*
 * The process control block. This can be created via `multitask_new()`.
 */
typedef struct process_block process_block_t;
struct process_block;

process_block_t* multitask_pb_get_next(process_block_t* p);
void multitask_pb_set_next(process_block_t* p, process_block_t* next);
hlogger_t* multitask_pb_get_logger(process_block_t* p);
char* multitask_pb_get_name(process_block_t* p);
uint64_t multitask_pb_get_pid(process_block_t* p);

struct multitask_state;
typedef struct multitask_state multitask_state_t;
void multitask_state_dump(multitask_state_t* mt);
uint64_t multitask_state_get_kernel_pid(multitask_state_t* mt);

void multitask_initialize(void);

/*
 * Creates a new process with the given entry address.
 * The memory allocated for the task is deallocated via a call
 * to `multitask_proc_terminate()`, or when the process finishes
 * and the scheduler cleans up. The caller must not call `free()`
 * on the process handle manually.
 */
process_block_t* multitask_proc_new(char* name, proc_entry_t entry, void* cr3);

/*
 * Adds a proc to the scheduler's queue.
 * The process will be added in a `READY_TO_RUN` state.
 */
void multitask_scheduler_add_proc(process_block_t* process);

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
 * Postpones task switches performed by the scheduler. Can be resumed with
 * `multitask_scheduler_resume()`.
 */
void multitask_scheduler_postpone(void);

/*
 * Resumes task switches performed by the scheduler previously postponed
 * with `multitask_scheduler_postpone()`.
 */
void multitask_scheduler_resume(void);

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

/*
 * Terminates a process. This deallocates all memory given to the process
 * and cleans up the process handle itself.
 */
void multitask_proc_terminate(process_block_t* p);

void* multitask_process_block_get_cr3(process_block_t* p);

#endif  // MULTITASK_H

