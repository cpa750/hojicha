[BITS 64]

global make_fork_kstack

make_fork_kstack:
    ; switch_to has already popped its saved-register frame and ret'd here.
    ; rsp points at the copied interrupt_frame_t on the child kernel stack.
    add rsp, 0x08

    pop rax
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rdi
    pop rsi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 0x10

    sti
    iretq
