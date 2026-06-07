#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

typedef struct spinlock {
  volatile uint32_t locked;
} spinlock_t;

void spinlock_init(spinlock_t* lock);
uint64_t spinlock_lock(spinlock_t* lock);
void spinlock_unlock(spinlock_t* lock, uint64_t irq_state);

#endif  // SPINLOCK_H
