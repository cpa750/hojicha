#ifndef KERNEL_STATE_H
#define KERNEL_STATE_H

#include <cpu/tss.h>
#include <drivers/pit.h>
#include <kernel/multitask.h>
#include <stdint.h>

struct multitask_state;
typedef struct multitask_state multitask_state_t;

struct pmm_state;
typedef struct pmm_state pmm_state_t;

struct vga_state;
typedef struct vga_state vga_state_t;

struct vmm_state;
typedef struct vmm_state vmm_state_t;

struct tty_state;
typedef struct tty_state tty_state_t;

struct kernel_state {
  tss_t* tss;
  process_block_t* current_process;
  multitask_state_t* mt;
  pit_state_t* pit;
  pmm_state_t* pmm;
  tty_state_t* tty;
  vmm_state_t* vmm;
  vga_state_t* vga;
};
typedef struct kernel_state kernel_state_t;

extern kernel_state_t g_kernel;
void initialize_g_kernel();
void g_kernel_dump();

#endif  // KERNEL_STATE_H
