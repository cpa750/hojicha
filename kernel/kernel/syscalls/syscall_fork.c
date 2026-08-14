#include <cpu/isr.h>
#include <kernel/g_kernel.h>
#include <mp/fork.h>
#include <kernel/syscall_callbacks.h>

long syscall_fork(interrupt_frame_t* frame) {
  return fork_proc(g_kernel.current_process, frame);
}
