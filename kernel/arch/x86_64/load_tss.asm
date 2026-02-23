[BITS 64]

global load_tss

section .text

load_tss:
    mov ax, 0x28
    ltr ax

