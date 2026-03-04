[BITS 64]

global enter_ring3

enter_ring3:
    ; User data segment
    mov rdx, 0x20 
    or rdx, 3 ; | 3 here to allow user access
    push rdx
    
    ; RSP
    push rdi

    ; Flags
    mov rdx, 0x200 ; Enable interrupts
    push rdx

    ; User code segment
    mov rdx, 0x18
    or rdx, 3
    push rdx

    ; RIP
    push rsi

    ; Jump to entry
    iretq

