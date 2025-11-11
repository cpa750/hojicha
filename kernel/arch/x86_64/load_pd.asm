[BITS 64]

global load_pd

section .text

load_pd:
  mov cr3, rdi
  ret
