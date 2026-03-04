#include <cpu/isr.h>
#include <hlog.h>
#include <kernel/kernel_state.h>
#include <kernel/multitask.h>
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

void handle_fault(interrupt_frame_t* frame) {
  char* exception_str;
  if (frame->int_no < 19) {
    exception_str = error_messages[frame->int_no];
  } else {
    exception_str = "Reserved";
  }

  if (multitask_pb_get_pid(g_kernel.current_process) ==
      multitask_state_get_kernel_pid(g_kernel.mt)) {
    hlog_add(HLOG_FATAL,
             "%s exception at %x in kernel. Aborting...",
             exception_str,
             frame->rip);
    hlog_add(HLOG_DEBUG, "Error code: %b", frame->err_code);
    if (frame->int_no == 14) {
      uint64_t cr2 = 0;
      asm volatile("\t movq %%cr2,%0" : "=r"(cr2));
      hlog_add(HLOG_DEBUG, "CR2: %x", cr2);
    }
    hlog_commit();
    abort();
  } else {
    hlog_add(
        HLOG_ERROR,
        "%s exception at %x in process '%s' (PID: %d). Terminating process...",
        exception_str,
        frame->rip,
        multitask_pb_get_name(g_kernel.current_process),
        multitask_pb_get_pid(g_kernel.current_process));
    hlog_add(HLOG_DEBUG, "Error code: %b", frame->err_code);
    if (frame->int_no == 14) {
      uint64_t cr2 = 0;
      asm volatile("\t movq %%cr2,%0" : "=r"(cr2));
      hlog_add(HLOG_DEBUG, "CR2: %x", cr2);
    }
    hlog_commit();
    multitask_proc_terminate(g_kernel.current_process);
  }
  if (frame->int_no < 19) {
    printf("%s exception.\n", error_messages[frame->int_no]);
  } else if (frame->int_no < 32) {
    printf("Reserved exception.\n");
  }
}
