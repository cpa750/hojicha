#ifndef ISR_H
#define ISR_H

#include <haddr.h>

struct interrupt_frame {
  haddr_t cr2;
  haddr_t rax;
  haddr_t rbx;
  haddr_t rcx;
  haddr_t rdx;
  haddr_t rdi;
  haddr_t rsi;
  haddr_t r8;
  haddr_t r9;
  haddr_t r10;
  haddr_t r11;
  haddr_t r12;
  haddr_t r13;
  haddr_t r14;
  haddr_t r15;
  haddr_t err_code;
  haddr_t int_no;
  haddr_t rip;
  haddr_t cs;
  haddr_t rflags;
  haddr_t rsp;
  haddr_t ss;
} __attribute__((packed));
typedef struct interrupt_frame interrupt_frame_t;

#endif  // ISR_H

