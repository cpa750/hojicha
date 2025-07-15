global load_idt

section .text

extern idt_pointer
load_idt:
  lidt [idt_pointer]
  ret

