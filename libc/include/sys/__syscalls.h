#ifndef __HOJICHA_SYS_SYSCALLS_H
#define __HOJICHA_SYS_SYSCALLS_H

#define __HOJICHA_SYS_SYSCALL_READ      0x0
#define __HOJICHA_SYS_SYSCALL_WRITE     0x1
#define __HOJICHA_SYS_SYSCALL_OPEN      0x2
#define __HOJICHA_SYS_SYSCALL_CLOSE     0x3
#define __HOJICHA_SYS_SYSCALL_STAT      0x4
#define __HOJICHA_SYS_SYSCALL_FSTAT     0x5
#define __HOJICHA_SYS_SYSCALL_LSEEK     0x8
#define __HOJICHA_SYS_SYSCALL_NANOSLEEP 0x23
#define __HOJICHA_SYS_SYSCALL_EXIT      0x3C
#define __HOJICHA_SYS_SYSCALL_GETDENTS  0x4E
#define __HOJICHA_SYS_SYSCALL_MKDIR     0x53
#define __HOJICHA_SYS_SYSCALL_RMDIR     0x54
#define __HOJICHA_SYS_SYSCALL_UNLINK    0x57

#ifdef __cplusplus
extern "C" {
#endif

extern long __syscall0(long n);
extern long __syscall1(long n, long a1);
extern long __syscall2(long n, long a1, long a2);
extern long __syscall3(long n, long a1, long a2, long a3);
extern long __syscall4(long n, long a1, long a2, long a3, long a4);
extern long __syscall5(long n, long a1, long a2, long a3, long a4, long a5);
extern long
__syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6);

#ifdef __cplusplus
}
#endif

#endif  // __HOJICHA_SYS_SYSCALLS_H
