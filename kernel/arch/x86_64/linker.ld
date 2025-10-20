OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

ENTRY(kernel_main)

PHDRS
{
  limine_requests PT_LOAD ;
  text            PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ;
  rodata          PT_LOAD    FLAGS((1 << 2)) ;
  data            PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ;
  dynamic         PT_DYNAMIC FLAGS((1 << 1) | (1 << 2)) ;
}

SECTIONS
{
  . = 0xffffffff80000000;
  __kernel_start = .;

  .limine_requests : {
    KEEP(*(.limine_requests_start))
    KEEP(*(.limine_requests))
    KEEP(*(.limine_requests_end))
  } :limine_requests

  . = ALIGN(CONSTANT(MAXPAGESIZE));

  .text :
  {
    *(.text .text.*)
  } :text

  . = ALIGN(CONSTANT(MAXPAGESIZE));

  .rodata :
  {
    *(.rodata .rodata.*)
  } :rodata

  .note.gnu.build-id : {
    *(.note.gnu.build-id)
  } :rodata

  . = ALIGN(CONSTANT(MAXPAGESIZE));

  .data :
  {
    *(.data .data.*)
  } :data

  .bss :
  {
    *(.bss)
    *(COMMON)
  } :data

  __kernel_end = .;

  /DISCARD/ : {
    *(.eh_frame*)
    *(.note .note.*)
  }
}
