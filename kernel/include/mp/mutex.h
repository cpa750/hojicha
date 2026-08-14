#ifndef MUTEX_H
#define MUTEX_H

#include <stdbool.h>

struct mutex;
typedef struct mutex mutex_t;

/*
 * Creates a mutex. The caller is responsible for calling `mutex_destroy()`
 * when all processes are done with the mutex.
 */
mutex_t* mutex_create(void);

/*
 * Cleans up the mutex `m` created with `mutex_create()`. Any processes waiting
 * on the mutex will hang. A process that currently owns the mutex will continue
 * to execute as normal, but any attempts to call `mutex_lock()` or
 * `mutex_unlock()` on a destroyed mutex will result in unknown behaviour.
 */
void mutex_destroy(mutex_t* m);

/*
 * Locks the mutex `m` for the current process. If another process currently
 * owns the mutex, the calling process will block until the mutex becomes
 * available.
 */
void mutex_lock(mutex_t* m);

/*
 * Attempts to lock the mutex `m` for the current process. If another process
 * currently owns the mutex, this function will return `false`, otherwise
 * `true`.
 */
bool mutex_try_lock(mutex_t* m);

/*
 * Releases the mutex `m` for the current process.
 */
void mutex_unlock(mutex_t* m);

#endif  // MUTEX_H
