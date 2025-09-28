.set ALIGN,     1<<0
.set MEMINFO,   1<<1
.set FLAGS,     ALIGN | MEMINFO
.set MAGIC,     0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot.data, "aw"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# Initialize the stack
.section .bootstrap_stack, "aw", @nobits
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .bss, "aw", @nobits
    .align 4096
boot_page_directory:
    .skip 4096
boot_page_id_table:
    .skip 4096
boot_page_kernel_table:
    .skip 4096

# Kernel entry point
.section .multiboot.text, "a"
.global _start
.type _start, @function
_start:
    # Set up identity page table
    movl  $0, %esi
    movl  $(boot_page_id_table - 0xC0000000), %edi
    movl  $1024, %ecx
1:
    # Identity map first 1 MB
    cmpl  $0x00100000, %esi
    jl    map_identity

    # Set up kernel page table
    movl  $__kernel_start, %esi
    movl  $(boot_page_kernel_table - 0xC0000000), %edi
    movl  $1024, %ecx

2:
    # Kernel mapping
    cmpl  $__kernel_start, %esi
    jl    next_kernel_entry
    cmpl  $(__kernel_end - 0xC0000000), %esi
    jge   next_kernel_entry

map_kernel:
    movl  %esi, %edx
    orl   $0x001, %edx

    cmpl  $__text_start, %esi
    jl    kernel_check_rodata
    movl  %edx, (%edi)
    jmp   next_kernel_entry

kernel_check_rodata:
    cmpl  $__rodata_start, %esi
    jl    kernel_writable

kernel_writable:
    orl   $0x002, %edx

kernel_map_entry:
    movl  %edx, (%edi)
    jmp   next_kernel_entry

map_identity:
    movl  %esi, %edx
    orl   $0x003, %edx
    movl  %edx, (%edi)
    addl  $4096, %esi
    addl  $4, %edi
    loop  1b

next_kernel_entry:
    addl  $4096, %esi
    addl  $4, %edi
    loop  2b

    movl  $(boot_page_kernel_table - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 0
    movl  $(boot_page_kernel_table - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 768 * 4
    movl  $(boot_page_directory - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 1023 * 4

    movl  $(boot_page_directory - 0xC0000000), %ecx
    movl  %ecx, %cr3

    movl  %cr0, %ecx
    orl   $0x80010001, %ecx
1:  hlt
    jmp   1b
    movl  %ecx, %cr0


    # Absolute jump needed to transition to higher half kernel
    lea   4f, %ecx
    jmp   *%ecx

.section .text
4:
    movl  $(boot_page_id_table - 0xC0000000 + 0x003), boot_page_directory + 0
    # Flush TLB
    movl %cr3, %ecx
    movl %ecx, %cr3

    mov   $stack_top, %esp
    call  _init

    # Pass multiboot info structure
    push  %eax
    push  %ebx
    call  kernel_main

    cli
1:  hlt
    jmp   1b

