[BITS 32]

global load_gdt

section .text

; Thank you OSDev wiki, I'll yoink this
extern gdt_pointer
load_gdt:
   lgdt [gdt_pointer]

   ; Reload CS register containing code selector:
   jmp   0x08:.reload_CS
.reload_CS:
   ; Reload data segment registers:
   mov   ax, 0x10
   mov   ds, ax
   mov   es, ax
   mov   fs, ax
   mov   gs, ax
   mov   ss, ax
   ret


