#include <kernel/multiboot.h>
#include <stdint.h>

void initialize_pmm(multiboot_info_t* m_info);
void pmm_alloc_frame();
void pmm_free_frame(uint16_t* page);
void pmm_reserve_region(uint16_t idx, uint16_t len);

