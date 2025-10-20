#include <kernel/kernel_state.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <string.h>

kernel_state_t g_kernel;

void initialize_g_kernel() { memset(&g_kernel, 0x0, sizeof(kernel_state_t)); }
void g_kernel_dump() {
  // pmm_state_dump(g_kernel.pmm);
  // vmm_state_dump(g_kernel.vmm);
}

