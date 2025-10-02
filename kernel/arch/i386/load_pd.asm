[BITS 32]

global load_pd

section .text

load_pd:
  push  ebp
  mov   ebp, esp
  mov   eax, [esp+8]
  mov   cr3, eax
  mov   esp, ebp
  pop   ebp
  ret
