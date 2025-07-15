#ifndef ISR_H
#define ISR_H

#include <stdint.h>

typedef struct {
  uint32_t gs, fs, es, ds;
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
  uint32_t interrupt_number, err_code;
  uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) InterruptRegisters;

#endif  // ISR_H

