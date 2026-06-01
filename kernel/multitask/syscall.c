#include <cpu/isr.h>
#include <errno.h>
#include <haddr.h>
#include <hlog.h>
#include <multitask/syscall.h>
#include <multitask/syscall_callbacks.h>
#include <sys/__syscalls.h>

struct syscall {
  syscall_callback_t callback;
  uint8_t nargs;
};
typedef struct syscall syscall_t;

void syscall_handle(interrupt_frame_t* frame) {
  long ret = -1;
  switch (frame->rax) {
    case __HOJICHA_SYS_SYSCALL_READ:
      ret = syscall_read(frame->rdi, (void*)frame->rsi, frame->rdx);
      break;
    case __HOJICHA_SYS_SYSCALL_WRITE:
      ret = syscall_write(frame->rdi, (void*)frame->rsi, frame->rdx);
      break;
    case __HOJICHA_SYS_SYSCALL_OPEN:
      ret = syscall_open((const char*)frame->rdi, (unsigned int)frame->rsi);
      break;
    case __HOJICHA_SYS_SYSCALL_EXIT:
      ret = syscall_exit((int)frame->rdi);
      break;
    case __HOJICHA_SYS_SYSCALL_NANOSLEEP:
      ret = syscall_nanosleep((unsigned long)frame->rdi);
      break;
    default:
      hlog_write(HLOG_WARN, "Syscall %d is invalid.", frame->rax);
      ret = -ENOSYS;
      break;
  }
  frame->rax = (haddr_t)ret;
}
