#include <cpu/isr.h>
#include <stdio.h>
#include <stdlib.h>

char* error_messages[] = {"Divide-by-zero",
                          "Debug",
                          "Non-maskable interrupt",
                          "Breakpoint",
                          "Into detected overflow",
                          "OOB",
                          "Invalid Opcode",
                          "No coprocsesor",
                          "Double fault",
                          "Coprocessor segment overrun",
                          "Bad TSS",
                          "Segment not present",
                          "Stack fault",
                          "General protection fault",
                          "Page fault",
                          "Unknown interrupt",
                          "Coproccesor fault",
                          "Alignment check",
                          "Machine check"};

void handle_fault(InterruptRegisters r) {
  if (r.interrupt_number < 19) {
    printf("%s exception.\n", error_messages[r.interrupt_number]);
  } else if (r.interrupt_number < 32) {
    printf("Reserved exception.\n");
  }
  abort();
}

