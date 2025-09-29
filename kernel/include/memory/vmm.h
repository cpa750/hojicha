#ifndef VMM_H
#define VMM_H

#include <stdint.h>

void initialize_vmm();
uint32_t vmm_map(uint32_t virt, uint32_t phys, uint32_t flags);
uint32_t vmm_unmap(uint32_t virt);
uint32_t vmm_to_physical(uint32_t virt);

#endif  // VMM_H

