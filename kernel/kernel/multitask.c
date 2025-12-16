#include <haddr.h>
#include <kernel/kernel_state.h>
#include <kernel/multitask.h>
#include <memory/vmm.h>
#include <stdio.h>
#include <stdlib.h>

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
  kernel_process->next = NULL;
  g_kernel.current_process = kernel_process;
}

void multitask_new(process_block_t* process) {
  asm volatile("cli");
  switch_to(process);
  asm volatile("sti");
}

