[BITS 32]

global enable_paging

section .text

enable_paging:
  push  ebp
  mov   ebp, esp
  mov   eax, cr0
  or    eax, 0x80000000
  mov   cr0, eax
  mov   esp, ebp
  pop   ebp
  ret
