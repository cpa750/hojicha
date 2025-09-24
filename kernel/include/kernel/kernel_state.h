#ifndef KERNEL_STATE_H
#define KERNEL_STATE_H

struct pmm_state;
typedef struct pmm_state pmm_state_t;

struct kernel_state {
  pmm_state_t* pmm;
};
typedef struct kernel_state kernel_state_t;

extern kernel_state_t g_kernel;
void initialize_g_kernel();
void g_kernel_dump();

#endif  // KERNEL_STATE_H

