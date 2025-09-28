ENTRY(_start)

SECTIONS
{
  . = 2M;
  __kernel_start = .;

  .multiboot.data : {
      *(.multiboot.data)
  }

  .multiboot.text : {
      *(.multiboot.text)
  }

  . += 0xC0000000;
  .text ALIGN (4K) : AT (ADDR (.text) - 0xC0000000)
  {
    __text_start = .;
    *(.text)
    __text_end = .;
  }
  .rodata ALIGN (4K) : AT (ADDR (.rodata) - 0xC0000000)
  {
    __rodata_start = .;
    *(.rodata)
    __rodata_end = .;
  }
  .data ALIGN (4K) : AT (ADDR (.data) - 0xC0000000)
  {
    *(.data)
  }
  .bss ALIGN (4K) : AT (ADDR (.bss) - 0xC0000000)
  {
    *(COMMON)
    *(.bss)
    *(.bootstrap_stack)
  }
  __kernel_end = .;
}
