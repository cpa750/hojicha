#ifndef __HOJICHA_SYS_SYSCALLS_H
#define __HOJICHA_SYS_SYSCALLS_H

#define __HOJICHA_SYS_SYSCALL_EXIT 0x3C

static inline long __syscall0(long n) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(n) : "memory", "cc");
  return ret;
}

static inline long __syscall1(long n, long a1) {
  long ret;
  asm volatile("int $0x80" : "=a"(ret) : "a"(n), "D"(a1) : "memory", "cc");
  return ret;
}

static inline long __syscall2(long n, long a1, long a2) {
  long ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(n), "D"(a1), "S"(a2)
               : "memory", "cc");
  return ret;
}

static inline long __syscall3(long n, long a1, long a2, long a3) {
  long ret;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(n), "D"(a1), "S"(a2), "d"(a3)
               : "memory", "cc");
  return ret;
}

static inline long __syscall4(long n, long a1, long a2, long a3, long a4) {
  long ret;
  register long r10 asm("r10") = a4;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
               : "memory", "cc");
  return ret;
}

static inline long __syscall5(long n,
                              long a1,
                              long a2,
                              long a3,
                              long a4,
                              long a5) {
  long ret;
  register long r10 asm("r10") = a4;
  register long r8 asm("r8") = a5;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
               : "memory", "cc");
  return ret;
}

static inline long
__syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
  long ret;
  register long r10 asm("r10") = a4;
  register long r8 asm("r8") = a5;
  register long r9 asm("r9") = a6;
  asm volatile("int $0x80"
               : "=a"(ret)
               : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
               : "memory", "cc");
  return ret;
}

#endif  // __HOJICHA_SYS_SYSCALLS_H

