#include <haddr.h>
#include <hlog.h>
#include <multitask/syscall.h>
#include <multitask/syscall_callbacks.h>

#include "cpu/isr.h"

#define SYSCALL_EXIT      0x3C
#define SYSCALL_NANOSLEEP 0x23

struct syscall {
  syscall_callback_t callback;
  uint8_t nargs;
};
typedef struct syscall syscall_t;

void syscall_handle(interrupt_frame_t* frame) {
  long ret = -1;
  switch (frame->rax) {
    case SYSCALL_EXIT:
      ret = syscall_exit((int)frame->rdi);
      break;
    case SYSCALL_NANOSLEEP:
      ret = syscall_nanosleep((unsigned long)frame->rdi);
      break;
    default:
      hlog_write(HLOG_WARN, "Syscall %d is invalid.", frame->rax);
      break;
  }
  frame->rax = (haddr_t)ret;
}
