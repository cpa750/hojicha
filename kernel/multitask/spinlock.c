#include <multitask/spinlock.h>
#include <stddef.h>
#include <utils/irq.h>

void spinlock_init(spinlock_t* lock) {
  if (lock == NULL) { return; }
  lock->locked = 0;
}

uint64_t spinlock_lock(spinlock_t* lock) {
  if (lock == NULL) { return 0; }
  uint64_t irq_state = irq_store();
  while (__sync_lock_test_and_set(&lock->locked, 1)) {}
  return irq_state;
}

void spinlock_unlock(spinlock_t* lock, uint64_t irq_state) {
  __sync_lock_release(&lock->locked);
  irq_load(irq_state);
}
