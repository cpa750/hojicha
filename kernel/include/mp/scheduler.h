#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <mp/proc.h>
#include <stdbool.h>
#include <stdint.h>

struct sched_state;
typedef struct sched_state sched_state_t;
uint64_t sched_state_get_kernel_pid(sched_state_t* mt);
proc_t* sched_state_get_ready_head(sched_state_t* mt);
proc_t* sched_state_get_sleeping_head(sched_state_t* mt);
proc_t* sched_state_get_ready_to_die_head(sched_state_t* mt);

void sched_initialize(void);

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

#endif  // SCHEDULER_H
