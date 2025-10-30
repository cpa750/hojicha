#ifndef VMM_H
#define VMM_H

#include <haddr.h>

#define PAGE_PRESENT 0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER_ACCESIBLE 0x4

struct vmm_state;
typedef struct vmm_state vmm_state_t;
haddr_t vmm_state_get_first_available_vaddr(vmm_state_t* vmm_state);
haddr_t vmm_state_get_last_available_vaddr(vmm_state_t* vmm_state);
void vmm_state_dump(vmm_state_t* v);

void initialize_vmm();
haddr_t vmm_map_at_paddr(haddr_t virt, haddr_t phys, haddr_t flags);
haddr_t vmm_map_single(haddr_t virt, haddr_t flags);
haddr_t vmm_map(haddr_t virt, haddr_t size, haddr_t flags);
haddr_t vmm_unmap(haddr_t virt);
haddr_t vmm_to_physical(haddr_t virt);

#endif  // VMM_H

