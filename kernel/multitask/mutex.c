#include <multitask/mutex.h>
#include <multitask/semaphore.h>
#include <stdlib.h>

struct mutex {
  semaphore_t* semaphore;
};

mutex_t* mutex_create(void) {
  mutex_t* m = (mutex_t*)calloc(1, sizeof(mutex_t));

  if (m != NULL) {
    m->semaphore = (semaphore_t*)semaphore_create(1);
    if (m->semaphore == NULL) {
      free(m);
      return NULL;
    }
  }

  return m;
}

void mutex_destroy(mutex_t* m) {
  semaphore_destroy(m->semaphore);
  free(m);
}

void mutex_lock(mutex_t* m) { semaphore_lock(m->semaphore); }

bool mutex_try_lock(mutex_t* m) { return semaphore_try_lock(m->semaphore); }

void mutex_unlock(mutex_t* m) { semaphore_unlock(m->semaphore); }
