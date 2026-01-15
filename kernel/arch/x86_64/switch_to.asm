[BITS 64]

%define G_KERNEL_TSS                 0
%define G_KERNEL_CURRENT_PROCESS     8
%define PCB_CR3                      0
%define PCB_RSP                      8
%define PCB_STATUS_RUNNING           1
%define PCB_STATUS                   16
%define PCB_RSP0                     8
%define TSS_RSP0                     4

extern g_kernel
global switch_to

switch_to:
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rax, g_kernel
    mov rbx, [rax + G_KERNEL_CURRENT_PROCESS]
    mov rcx, cr3
    mov [rbx + PCB_CR3], rcx
    mov [rbx + PCB_RSP], rsp

    mov [rel g_kernel + G_KERNEL_CURRENT_PROCESS], rdi

    mov rsp,  [rdi + PCB_RSP]
    mov rax,  [rdi + PCB_CR3]
    mov byte [rdi + PCB_STATUS], PCB_STATUS_RUNNING
    cmp rax, rcx
    je .skip_cr3
    mov cr3, rax

.skip_cr3:
    mov rbx, [rdi + PCB_RSP0]
    mov rcx, [g_kernel + G_KERNEL_TSS]
    mov [rcx + TSS_RSP0], rbx

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ret
