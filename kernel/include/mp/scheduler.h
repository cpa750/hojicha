#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <cpu/isr.h>
#include <mp/proc.h>
#include <stdbool.h>
#include <stdint.h>

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
proc_t* sched_kproc_new(char* name, proc_entry_t entry, void* cr3);

/*
 * Creates a new user space process with the given entry address.
 * The memory allocated for the task is deallocated via a call
 * to `sched_proc_terminate()`, or when the process finishes
 * and the scheduler cleans up. The caller must not call `free()`
 * on the process handle manually.
 */
proc_t* sched_uproc_new(char* name, elf_t* elf);

/*
 * Replaces the current running executable with a new one.
 */
long sched_execve(proc_t* process,
                  elf_t* elf,
                  char* name,
                  uint64_t name_len,
                  uint64_t argc,
                  char** argv,
                  char** envp);

/*
 * Forks the given `process`.
 */
long sched_fork(proc_t* process, interrupt_frame_t* frame);

/*
 * Waits for a child process to exit and reaps it.
 */
long sched_waitpid(proc_t* process,
                   long pid,
                   int* wstatus,
                   int options);

/*
 * Adds a proc to the scheduler's queue.
 * The process will be added in a `READY_TO_RUN` state.
 */
void sched_add_proc(proc_t* process);

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

/*
 * Voluntarily yields the CPU to another runnable process, if one exists.
 */
void sched_yield(void);

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
void sched_proc_unblock(proc_t* process);

/*
 * Sleeps the current process `s` seconds.
 */
void sched_current_sleep(uint64_t s);

/*
 * Sleeps the current process `ns` nanoseconds.
 */
void sched_current_sleep_ns(uint64_t ns);

/*
 * Terminates a process. Child processes with live parents remain waitable
 * until the parent calls waitpid().
 */
void sched_proc_terminate(proc_t* p);

/*
 * Terminates a process with an exit status visible to waitpid().
 */
void sched_proc_exit(proc_t* p, int code);

#endif  // SCHEDULER_H
