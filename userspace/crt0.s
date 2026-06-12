.global _start
.type _start, @function
.extern main
.extern exit

_start:
  mov (%rsp), %rdi
  lea 8(%rsp), %rsi
  mov %rsi, %rdx

1:
  cmpq $0, (%rdx)
  je 2f
  add $8, %rdx
  jmp 1b

2:
  add $8, %rdx
  call main
  mov %rax, %rdi
  call exit

3:
  jmp 3b

.size _start, . - _start

.section .note.GNU-stack,"",@progbits
