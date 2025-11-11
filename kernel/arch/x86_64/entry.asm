bits 64

global _start
extern kernel_main
extern __stack_end

section .text

_start:
  mov rax, __stack_end
  and rax, -16
  mov rsp, rax
  xor rbp, rbp
  jmp kernel_main

