.global _start
.type _start, @function
.extern main
.extern exit

_start:
  call main
  mov %eax, %edi
  call exit

1:
  jmp 1b

.size _start, . - _start

.section .note.GNU-stack,"",@progbits
