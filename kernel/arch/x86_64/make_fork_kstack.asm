[BITS 64]

global make_fork_kstack

make_fork_kstack:
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
