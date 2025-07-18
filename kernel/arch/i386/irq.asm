extern handle_irq

%macro irq_stub 1
irq_stub_%+%1:
    push byte 0
    push byte %1
    jmp irq_common
%endmacro

irq_stub 32

irq_common:
    pusha
    push gs
    push fs
    push es
    push ds
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov eax, esp
    call handle_irq
    pop ds
    pop es
    pop fs
    pop gs
    popa
    add esp, 8
    iret

global irq_stub_table
irq_stub_table:
    dd irq_stub_32

