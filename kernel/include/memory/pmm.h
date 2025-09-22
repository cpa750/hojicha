#include <kernel/multiboot.h>
#include <stdint.h>

void initialize_pmm(multiboot_info_t* m_info);
uint32_t pmm_alloc_frame();
void pmm_free_frame(uint32_t addr);
void pmm_reserve_region(uint16_t idx, uint16_t len);

