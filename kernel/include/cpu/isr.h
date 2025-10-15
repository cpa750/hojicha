#ifndef ISR_H
#define ISR_H

#include <haddr.h>

typedef struct {
  haddr_t gs, fs, es, ds;
  haddr_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
  haddr_t interrupt_number, err_code;
  haddr_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) InterruptRegisters;

#endif  // ISR_H

