#include <kernel/kernel_state.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <string.h>

#define PAGE_PRESENT 0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER_ACCESIBLE 0x4

#define KERNEL_VIRT_START 0xC0000000
#define KERNEL_PD_START_IDX KERNEL_VIRT_START >> 22

void initialize_vmm() {
  uint32_t* page_directory = (uint32_t*)pmm_alloc_frame();
  memset(page_directory, 0, 4096);

  uint32_t* pd_identity_entry = (uint32_t*)pmm_alloc_frame();
  for (uint32_t addr = 0; addr < 0x100000; addr += 4096) {
    pd_identity_entry[addr >> 12] = addr | PAGE_PRESENT | PAGE_WRITABLE;
  }
  page_directory[0] = (uint32_t)pd_identity_entry;

  uint32_t* pd_kernel_entry = (uint32_t*)pmm_alloc_frame();
  memset(pd_kernel_entry, 0, 4096);
  uint32_t kernel_page_count = pmm_state_get_kernel_page_count(g_kernel.pmm);
  for (uint32_t kernel_page = 0; kernel_page < kernel_page_count;
       ++kernel_page) {
    pd_kernel_entry[kernel_page] =
        (kernel_page << 12) | PAGE_PRESENT | PAGE_WRITABLE;
  }
  page_directory[KERNEL_PD_START_IDX] = (uint32_t)pd_kernel_entry;
}

void vmm_map(uint32_t virt, uint32_t phys) {}

void vmm_unmap(uint32_t virt, uint32_t phys) {}

