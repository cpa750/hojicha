[BITS 32]

global load_gdt

section .text

; Thank you OSDev wiki, I'll yoink this
load_gdt:
   mov eax, [esp+4]
   lgdt [eax]

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


