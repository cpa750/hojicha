#include <haddr.h>
#include <kernel/kernel_state.h>
#include <kernel/multitask.h>
#include <memory/vmm.h>
#include <stdlib.h>

#define STACK_SIZE 16384

struct multitask_state {
  process_block_t* last;
  uint64_t time_elapsed;
};

static multitask_state_t mt = {0};

extern void switch_to(process_block_t* process);

void multitask_initialize(void) {
  process_block_t* kernel_process =
      (process_block_t*)malloc(sizeof(process_block_t));
  haddr_t cr3;
  haddr_t rsp;
  asm volatile("\t movq %%cr3,%0" : "=r"(cr3));
  asm volatile("\t movq %%rsp,%0" : "=r"(rsp));
  kernel_process->cr3 = (void*)cr3;
  kernel_process->rsp = (void*)rsp;
  kernel_process->status = 0;
  kernel_process->next = kernel_process;
  mt.last = kernel_process;
  g_kernel.mt = &mt;
  g_kernel.current_process = kernel_process;
}

process_block_t* multitask_new(proc_entry_t entry, void* cr3) {
  // TODO figure out a way of ending processes
  process_block_t* new_proc = (process_block_t*)malloc(sizeof(process_block_t));
  new_proc->cr3 = cr3;
  uint8_t* new_stack_end = (uint8_t*)malloc(STACK_SIZE);
  haddr_t stack_base = (haddr_t)(new_stack_end + STACK_SIZE);
  stack_base &= ~0xFULL;
  stack_base -= 8;
  *(haddr_t*)stack_base = (haddr_t)entry;

  // Because we pop 15 registers from the newly allocated stack
  // in switch_to()
  stack_base -= 8 * 15;
  new_proc->rsp = (void*)stack_base;
  return new_proc;
}

void multitask_schedule_add_proc(process_block_t* process) {
  if (g_kernel.mt->last == NULL) { return; }
  if (g_kernel.current_process == NULL) {
    g_kernel.current_process = process;
    process->next = process;
    return;
  }

  process->next = g_kernel.mt->last->next;
  g_kernel.mt->last->next = process;
  g_kernel.mt->last = process;
}

void multitask_schedule() {
  // TODO What happens to timekeeping if the switch fails?
  g_kernel.mt->time_elapsed = pit_get_ns_elapsed_since_init(g_kernel.pit);
  g_kernel.current_process->elapsed +=
      g_kernel.mt->time_elapsed - g_kernel.current_process->switch_timestamp;
  g_kernel.current_process->next->switch_timestamp = g_kernel.mt->time_elapsed;

  multitask_switch(g_kernel.current_process->next);
}

void multitask_switch(process_block_t* process) {
  asm volatile("cli");
  switch_to(process);
  asm volatile("sti");
}

