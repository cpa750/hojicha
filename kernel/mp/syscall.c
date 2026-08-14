#include <cpu/isr.h>
#include <errno.h>
#include <haddr.h>
#include <hlog.h>
#include <internal/__syscalls.h>
#include <kernel/syscall.h>
#include <kernel/syscall_callbacks.h>

struct syscall {
  syscall_callback_t callback;
  uint8_t nargs;
};
typedef struct syscall syscall_t;

static const char* syscall_name(unsigned long number) {
  switch (number) {
    case __HOJICHA_INTERNAL_SYSCALL_READ:
      return "read";
    case __HOJICHA_INTERNAL_SYSCALL_WRITE:
      return "write";
    case __HOJICHA_INTERNAL_SYSCALL_OPEN:
      return "open";
    case __HOJICHA_INTERNAL_SYSCALL_CLOSE:
      return "close";
    case __HOJICHA_INTERNAL_SYSCALL_STAT:
      return "stat";
    case __HOJICHA_INTERNAL_SYSCALL_FSTAT:
      return "fstat";
    case __HOJICHA_INTERNAL_SYSCALL_LSTAT:
      return "lstat";
    case __HOJICHA_INTERNAL_SYSCALL_LSEEK:
      return "lseek";
    case __HOJICHA_INTERNAL_SYSCALL_MMAP:
      return "mmap";
    case __HOJICHA_INTERNAL_SYSCALL_MUNMAP:
      return "munmap";
    case __HOJICHA_INTERNAL_SYSCALL_BRK:
      return "brk";
    case __HOJICHA_INTERNAL_SYSCALL_IOCTL:
      return "ioctl";
    case __HOJICHA_INTERNAL_SYSCALL_DUP2:
      return "dup2";
    case __HOJICHA_INTERNAL_SYSCALL_NANOSLEEP:
      return "nanosleep";
    case __HOJICHA_INTERNAL_SYSCALL_FORK:
      return "fork";
    case __HOJICHA_INTERNAL_SYSCALL_EXECVE:
      return "execve";
    case __HOJICHA_INTERNAL_SYSCALL_EXIT:
      return "exit";
    case __HOJICHA_INTERNAL_SYSCALL_WAITPID:
      return "waitpid";
    case __HOJICHA_INTERNAL_SYSCALL_GETDENTS:
      return "getdents";
    case __HOJICHA_INTERNAL_SYSCALL_GETCWD:
      return "getcwd";
    case __HOJICHA_INTERNAL_SYSCALL_CHDIR:
      return "chdir";
    case __HOJICHA_INTERNAL_SYSCALL_FCHDIR:
      return "fchdir";
    case __HOJICHA_INTERNAL_SYSCALL_MKDIR:
      return "mkdir";
    case __HOJICHA_INTERNAL_SYSCALL_RMDIR:
      return "rmdir";
    case __HOJICHA_INTERNAL_SYSCALL_LINK:
      return "link";
    case __HOJICHA_INTERNAL_SYSCALL_UNLINK:
      return "unlink";
    case __HOJICHA_INTERNAL_SYSCALL_SYMLINK:
      return "symlink";
    case __HOJICHA_INTERNAL_SYSCALL_READLINK:
      return "readlink";
    case __HOJICHA_INTERNAL_SYSCALL_CLOCK_GETTIME:
      return "clock_gettime";
    default:
      return "unknown";
  }
}

void syscall_handle(interrupt_frame_t* frame) {
  long ret = -1;
  const char* name = syscall_name(frame->rax);
  hlog_write(HLOG_DEBUG,
             "syscall start %s(%x, %x, %x, %x, %x, %x)",
             name,
             frame->rdi,
             frame->rsi,
             frame->rdx,
             frame->r10,
             frame->r8,
             frame->r9);

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
    case __HOJICHA_INTERNAL_SYSCALL_MMAP:
      ret = syscall_mmap((void*)frame->rdi,
                         frame->rsi,
                         (int)frame->rdx,
                         (int)frame->r10,
                         (int)frame->r8,
                         (long)frame->r9);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_MUNMAP:
      ret = syscall_munmap((void*)frame->rdi, frame->rsi);
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
    case __HOJICHA_INTERNAL_SYSCALL_CLOCK_GETTIME:
      ret =
          syscall_clock_gettime((clockid_t)frame->rdi,
                                (struct timespec*)frame->rsi);
      break;
    case __HOJICHA_INTERNAL_SYSCALL_BRK:
      ret = syscall_brk((unsigned long)frame->rdi);
      break;
    default:
      hlog_write(HLOG_WARN, "Syscall %d is invalid.", frame->rax);
      ret = -ENOSYS;
      break;
  }
  hlog_write(HLOG_DEBUG, "syscall end %s -> %x", name, ret);
  frame->rax = (haddr_t)ret;
}
