#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define PAGE_PRESENT 0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER_ACCESIBLE 0x4

struct vmm_state;
typedef struct vmm_state vmm_state_t;
uint32_t vmm_state_get_first_available_vaddr(vmm_state_t* vmm_state);
uint32_t vmm_state_get_last_available_vaddr(vmm_state_t* vmm_state);
void vmm_state_dump(vmm_state_t* v);

void initialize_vmm();
uint32_t vmm_map_single(uint32_t virt, uint32_t flags);
uint32_t vmm_map(uint32_t virt, uint32_t size, uint32_t flags);
uint32_t vmm_unmap(uint32_t virt);
uint32_t vmm_to_physical(uint32_t virt);

#endif  // VMM_H

