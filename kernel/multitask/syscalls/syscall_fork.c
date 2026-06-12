#include <cpu/isr.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>

long syscall_fork(interrupt_frame_t* frame) {
  return sched_fork(g_kernel.current_process, frame);
}
