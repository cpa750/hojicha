#include <cpu/isr.h>
#include <errno.h>
#include <haddr.h>
#include <hlog.h>
#include <internal/__syscalls.h>
#include <multitask/syscall.h>
#include <multitask/syscall_callbacks.h>

struct syscall {
  syscall_callback_t callback;
  uint8_t nargs;
};
typedef struct syscall syscall_t;

void syscall_handle(interrupt_frame_t* frame) {
  long ret = -1;
  switch (frame->rax) {
    case __HOJICHA_INTERNAL_SYSCALL_READ:
      ret = syscall_read(frame->rdi, (void*)frame->rsi, frame->rdx);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_WRITE:
      ret = syscall_write(frame->rdi, (void*)frame->rsi, frame->rdx);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_OPEN:
      ret = syscall_open((const char*)frame->rdi, (unsigned int)frame->rsi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_CLOSE:
      ret = syscall_close(frame->rdi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_FORK:
      ret = syscall_fork(frame);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_EXECVE:
      ret = syscall_execve((const char*)frame->rdi,
                           (char* const*)frame->rsi,
                           (char* const*)frame->rdx);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_STAT:
      ret = syscall_stat((const char*)frame->rdi, (stat_t*)frame->rsi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_FSTAT:
      ret = syscall_fstat(frame->rdi, (stat_t*)frame->rsi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_LSTAT:
      ret = syscall_lstat((const char*)frame->rdi, (stat_t*)frame->rsi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_LSEEK:
      ret = syscall_lseek(frame->rdi, (long)frame->rsi, (int)frame->rdx);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_IOCTL:
      ret = syscall_ioctl(frame->rdi, frame->rsi, (void*)frame->rdx);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_DUP2:
      ret = syscall_dup2(frame->rdi, frame->rsi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_GETDENTS:
      ret =
          syscall_getdents(frame->rdi, (linux_dirent_t*)frame->rsi, frame->rdx);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_GETCWD:
      ret = syscall_getcwd((char*)frame->rdi, frame->rsi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_CHDIR:
      ret = syscall_chdir((const char*)frame->rdi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_FCHDIR:
      ret = syscall_fchdir(frame->rdi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_MKDIR:
      ret = syscall_mkdir((const char*)frame->rdi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_RMDIR:
      ret = syscall_rmdir((const char*)frame->rdi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_LINK:
      ret = syscall_link((const char*)frame->rdi, (const char*)frame->rsi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_UNLINK:
      ret = syscall_unlink((const char*)frame->rdi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_SYMLINK:
      ret = syscall_symlink((const char*)frame->rdi, (const char*)frame->rsi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_READLINK:
      ret = syscall_readlink(
          (const char*)frame->rdi, (char*)frame->rsi, (long)frame->rdx);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_EXIT:
      ret = syscall_exit((int)frame->rdi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_WAITPID:
      ret = syscall_waitpid((long)frame->rdi, (int*)frame->rsi, frame->rdx);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_NANOSLEEP:
      ret = syscall_nanosleep((unsigned long)frame->rdi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_BRK:
      ret = syscall_brk((unsigned long)frame->rdi);
      break;
    default:
      hlog_write(HLOG_WARN, "Syscall %d is invalid.", frame->rax);
      ret = -ENOSYS;
      break;
  }
  frame->rax = (haddr_t)ret;
}
