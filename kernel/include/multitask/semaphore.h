#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdbool.h>
#include <stdint.h>

struct semaphore;
typedef struct semaphore semaphore_t;

/*
 * Creates a semaphore. `limit` processes can access the
 * semaphore at a time. The caller is responsible for calling
 * `semaphore_destroy()` when all processes are done with the semaphore.
 */
semaphore_t* semaphore_create(uint8_t limit);

/*
 * Cleans up the semaphore `s` created with `semaphore_create()`. Any processes
 * waiting on the semaphore will hang. A process that currently owns the
 * semaphore will continue to execute as normal, but any attempts to call
 * `semaphore_lock()` or `semaphore_unlock()` on a destroyed semaphore will
 * result in unknown behaviour.
 */
void semaphore_destroy(semaphore_t* s);

/*
 * Locks the semaphore `s` for the current process. If another process currently
 * owns the semaphore, the calling process will block until the semaphore
 * becomes available.
 */
void semaphore_lock(semaphore_t* s);

/*
 * Attempts to lock the semaphore `s` for the current process. If another
 * process currently owns the semaphore, this function will return `false`,
 * otherwise `true`.
 */
bool semaphore_try_lock(semaphore_t* s);

/*
 * Releases the semaphore `s` for the current process.
 */
void semaphore_unlock(semaphore_t* s);

#endif  // SEMAPHORE_H
