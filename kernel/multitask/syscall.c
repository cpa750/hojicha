#include <cpu/isr.h>
#include <errno.h>
#include <haddr.h>
#include <hlog.h>
#include <multitask/syscall.h>
#include <multitask/syscall_callbacks.h>

#define SYSCALL_READ      0x0
#define SYSCALL_WRITE     0x1
#define SYSCALL_OPEN      0x2
#define SYSCALL_NANOSLEEP 0x23
#define SYSCALL_EXIT      0x3C

struct syscall {
  syscall_callback_t callback;
  uint8_t nargs;
};
typedef struct syscall syscall_t;

void syscall_handle(interrupt_frame_t* frame) {
  long ret = -1;
  switch (frame->rax) {
    case SYSCALL_READ:
      ret = syscall_read(frame->rdi, (void*)frame->rsi, frame->rdx);
      break;
    case SYSCALL_WRITE:
      ret = syscall_write(frame->rdi, (void*)frame->rsi, frame->rdx);
    case SYSCALL_OPEN:
      ret = syscall_open((const char*)frame->rdi, (unsigned int)frame->rsi);
      break;
    case SYSCALL_EXIT:
      ret = syscall_exit((int)frame->rdi);
      break;
    case SYSCALL_NANOSLEEP:
      ret = syscall_nanosleep((unsigned long)frame->rdi);
      break;
    default:
      hlog_write(HLOG_WARN, "Syscall %d is invalid.", frame->rax);
      ret = -ENOSYS;
      break;
  }
  frame->rax = (haddr_t)ret;
}
