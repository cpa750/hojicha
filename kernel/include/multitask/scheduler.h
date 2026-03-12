#ifndef MULTITASK_H
#define MULTITASK_H

#include <haddr.h>
#include <hlog.h>
#include <memory/vmm.h>
#include <stdbool.h>
#include <stdint.h>

#define PROC_STATUS_PAUSED    0b00000100
#define PROC_STATUS_SEMAPHORE 0b00001000
#define STACK_SIZE            16384  // 4 pages

typedef struct elf elf_t;

/*
 * The entry point of the process. Must take no parameters and return void.
 */
typedef void (*proc_entry_t)(void);

/*
 * The process control block. This can be created via `sched_kproc_new()` for
 * kernel processes or `sched_uproc_new()` for userland processes.
 */
typedef struct process_block process_block_t;
struct process_block;

process_block_t* sched_pb_get_next(process_block_t* p);
void sched_pb_set_next(process_block_t* p, process_block_t* next);
hlogger_t* sched_pb_get_logger(process_block_t* p);
char* sched_pb_get_name(process_block_t* p);
uint64_t sched_pb_get_pid(process_block_t* p);
vmm_t* sched_pb_get_vmm(process_block_t* p);
void sched_pb_set_elf(process_block_t* p, elf_t* elf);
void* sched_pb_get_cr3(process_block_t* p);

struct sched_state;
typedef struct sched_state sched_state_t;
void sched_state_dump(sched_state_t* mt);
uint64_t sched_state_get_kernel_pid(sched_state_t* mt);

void sched_initialize(void);

/*
 * Creates a new kernel process with the given entry address.
 * The memory allocated for the task is deallocated via a call
 * to `sched_proc_terminate()`, or when the process finishes
 * and the scheduler cleans up. The caller must not call `free()`
 * on the process handle manually.
 */
process_block_t* sched_kproc_new(char* name, proc_entry_t entry, void* cr3);

/*
 * Creates a new user space process with the given entry address.
 * The memory allocated for the task is deallocated via a call
 * to `sched_proc_terminate()`, or when the process finishes
 * and the scheduler cleans up. The caller must not call `free()`
 * on the process handle manually.
 */
process_block_t* sched_uproc_new(char* name, elf_t* elf);

/*
 * Adds a proc to the scheduler's queue.
 * The process will be added in a `READY_TO_RUN` state.
 */
void sched_add_proc(process_block_t* process);

/*
 * Advances the scheduler if there is an available next process.
 * Updates the current proc's elapsed counter, sets the next proc's switch
 * timestamp, and switches to the new proc. If the current process is being
 * pre-empted, it is added to the ready to run queue.
 * The callee is required to lock before, and unlock after with the
 * `sched_lock()` and `sched_unlock()`
 * functions.
 */
void schedule_advance(void);

void sched_lock(void);
void sched_unlock(void);

/*
 * Postpones task switches performed by the scheduler. Can be resumed with
 * `sched_resume()`.
 */
void sched_postpone(void);

/*
 * Resumes task switches performed by the scheduler previously postponed
 * with `sched_postpone()`.
 */
void sched_resume(void);

/*
 * Blocks the current process with the given `reason`.
 */
void sched_current_block(uint8_t reason);

/*
 * Unblocks the given process. Only pre-empts if the process is the only one
 * running, otherwise the process is appended to the scheduler's queue.
 */
void sched_proc_unblock(process_block_t* process);

/*
 * Sleeps the current process `s` seconds.
 */
void sched_current_sleep(uint64_t s);

/*
 * Sleeps the current process `ns` nanoseconds.
 */
void sched_current_sleep_ns(uint64_t ns);

/*
 * Terminates a process. This deallocates all memory given to the process
 * and cleans up the process handle itself.
 */
void sched_proc_terminate(process_block_t* p);

#endif  // MULTITASK_H

