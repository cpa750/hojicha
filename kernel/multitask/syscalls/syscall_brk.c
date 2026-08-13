#include <hlog.h>
#include <kernel/g_kernel.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>
#include <stddef.h>

unsigned long syscall_brk(unsigned long brk) {
  process_mem_t* mem = sched_pb_get_mem(g_kernel.current_process);
  hlog_write(HLOG_VERBOSE, "syscall_brk request brk=%x", brk);

  if (mem == NULL || mem->vmm == NULL) {
    hlog_write(HLOG_VERBOSE, "syscall_brk return 0 missing memory state");
    return 0;
  }

  if (brk == 0) {
    hlog_write(HLOG_VERBOSE, "syscall_brk query return=%x", mem->brk);
    return mem->brk;
  }

  haddr_t new_brk = (haddr_t)brk;
  if (mem->stack_start <= PAGE_SIZE) {
    hlog_write(HLOG_VERBOSE,
               "syscall_brk reject stack_start=%x return=%x",
               mem->stack_start,
               mem->brk);
    return mem->brk;
  }

  haddr_t stack_guard_start = mem->stack_start - PAGE_SIZE;
  if (new_brk < mem->brk_start || new_brk > stack_guard_start) {
    hlog_write(HLOG_VERBOSE,
               "syscall_brk reject range request=%x brk_start=%x stack_guard=%x return=%x",
               new_brk,
               mem->brk_start,
               stack_guard_start,
               mem->brk);
    return mem->brk;
  }

  haddr_t old_page = pmm_addr_to_page(mem->brk);
  haddr_t new_page = pmm_addr_to_page_ceil(new_brk);
  if (new_page > old_page) {
    haddr_t page_count = new_page - old_page;
    haddr_t map_start = pmm_page_to_addr_base(old_page);
    hlog_write(HLOG_VERBOSE,
               "syscall_brk map start=%x pages=%x old_brk=%x new_brk=%x",
               map_start,
               page_count,
               mem->brk,
               new_brk);
    if (vmm_map(mem->vmm,
                map_start,
                page_count,
                PAGE_PRESENT | PAGE_USER_ACCESIBLE | PAGE_WRITABLE) == 0) {
      hlog_write(HLOG_VERBOSE,
                 "syscall_brk map failed start=%x pages=%x return=%x",
                 map_start,
                 page_count,
                 mem->brk);
      return mem->brk;
    }
  }

  mem->brk = new_brk;
  hlog_write(HLOG_VERBOSE, "syscall_brk success return=%x", mem->brk);
  return mem->brk;
}
