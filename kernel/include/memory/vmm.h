#ifndef VMM_H
#define VMM_H

#include <stdint.h>

void initialize_vmm();
void vmm_map(uint32_t virt, uint32_t phys);
void vmm_unmap(uint32_t virt, uint32_t phys);

#endif  // VMM_H

