[BITS 64]

section .text

global __syscall0
__syscall0:
  mov rax, rdi
  int 0x80
  ret

global __syscall1
__syscall1:
  mov rax, rdi
  mov rdi, rsi
  int 0x80
  ret

global __syscall2
__syscall2:
  mov rax, rdi
  mov rdi, rsi
  mov rsi, rdx
  int 0x80
  ret

global __syscall3
__syscall3:
  mov rax, rdi
  mov rdi, rsi
  mov rsi, rdx
  mov rdx, rcx
  int 0x80
  ret

global __syscall4
__syscall4:
  mov rax, rdi
  mov rdi, rsi
  mov rsi, rdx
  mov rdx, rcx
  mov r10, r8
  int 0x80
  ret

global __syscall5
__syscall5:
  mov rax, rdi
  mov rdi, rsi
  mov rsi, rdx
  mov rdx, rcx
  mov r10, r8
  int 0x80
  ret

global __syscall6
__syscall6:
  mov rax, rdi
  mov rdi, rsi
  mov rsi, rdx
  mov rdx, rcx
  mov r10, r8
  mov r8, [rsp + 8]
  int 0x80
  ret
