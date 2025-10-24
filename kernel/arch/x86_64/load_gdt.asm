[BITS 64]

global load_gdt

section .text

extern gdt_pointer
load_gdt:
   lgdt [gdt_pointer]

   ; Reload CS register containing code selector:
   push 0x08
   push .reload_CS
   retfq

.reload_CS:
   ; Reload data segment registers:
   mov   ax, 0x10
   mov   ds, ax
   mov   es, ax
   mov   fs, ax
   mov   gs, ax
   mov   ss, ax
   ret


