#ifndef PMM_H
#define PMM_H

#include <haddr.h>
#include <kernel/multiboot.h>
#include <stdint.h>

struct pmm_state;
typedef struct pmm_state pmm_state_t;
haddr_t pmm_state_get_total_mem(pmm_state_t*);
haddr_t pmm_state_get_free_mem(pmm_state_t*);
haddr_t pmm_state_get_total_pages(pmm_state_t*);
haddr_t pmm_state_get_free_pages(pmm_state_t*);
haddr_t pmm_state_get_page_size(pmm_state_t*);
haddr_t pmm_state_get_mem_bitmap(pmm_state_t*);
haddr_t pmm_state_get_kernel_start(pmm_state_t*);
haddr_t pmm_state_get_kernel_vstart(pmm_state_t*);
haddr_t pmm_state_get_kernel_end(pmm_state_t*);
haddr_t pmm_state_get_kernel_vend(pmm_state_t*);
haddr_t pmm_state_get_kernel_page_count(pmm_state_t*);
void pmm_state_dump(pmm_state_t*);

void initialize_pmm();
void pmm_initialize_bitmap();
haddr_t pmm_alloc_frame();
void pmm_free_frame(haddr_t addr);
haddr_t pmm_addr_to_page(haddr_t addr);
haddr_t pmm_page_to_addr_base(haddr_t page);

#endif  // PMM_H

