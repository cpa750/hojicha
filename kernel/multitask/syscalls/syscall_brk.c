#include <kernel/g_kernel.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>
#include <stddef.h>

unsigned long syscall_brk(unsigned long brk) {
  process_mem_t* mem = sched_pb_get_mem(g_kernel.current_process);
  if (mem == NULL || mem->vmm == NULL) { return 0; }

  if (brk == 0) { return mem->brk; }

  haddr_t new_brk = (haddr_t)brk;
  if (mem->stack_start <= PAGE_SIZE) { return mem->brk; }

  haddr_t stack_guard_start = mem->stack_start - PAGE_SIZE;
  if (new_brk < mem->brk_start || new_brk > stack_guard_start) {
    return mem->brk;
  }

  haddr_t old_page = pmm_addr_to_page(mem->brk);
  haddr_t new_page = pmm_addr_to_page_ceil(new_brk);
  if (new_page > old_page) {
    haddr_t page_count = new_page - old_page;
    haddr_t map_start = pmm_page_to_addr_base(old_page);
    if (vmm_map(mem->vmm,
                map_start,
                page_count,
                PAGE_PRESENT | PAGE_USER_ACCESIBLE | PAGE_WRITABLE) == 0) {
      return mem->brk;
    }
  }

  mem->brk = new_brk;
  return mem->brk;
}
