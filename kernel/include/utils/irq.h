#ifndef HOJICHA_UTILS_IRQ_H
#define HOJICHA_UTILS_IRQ_H

#include <stdint.h>

#define IRQ_RFLAGS_IF (1ULL << 9)

static inline uint64_t irq_store(void) {
  uint64_t irq_state;
  asm volatile("pushfq; popq %0; cli" : "=r"(irq_state)::"memory");
  return irq_state;
}

static inline void irq_load(uint64_t irq_state) {
  if ((irq_state & IRQ_RFLAGS_IF) != 0) { asm volatile("sti" ::: "memory"); }
}

#endif  // HOJICHA_UTILS_IRQ_H
