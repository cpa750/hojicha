[BITS 64]

global memcpy

memcpy:
  mov rcx, rdx
  rep movsb
  sub rdi, rdx
  mov rax, rdi
  ret

