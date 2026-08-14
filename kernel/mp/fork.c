#include <cpu/fpu.h>
#include <errno.h>
#include <haddr.h>
#include <hlog.h>
#include <kernel/g_kernel.h>
#include <memory/vmm.h>
#include <mp/fork.h>
#include <stdint.h>
#include <string.h>

#include "mp_utils.h"

extern void make_fork_kstack(void);

long fork_proc(proc_t* process, interrupt_frame_t* frame) {
  if (process == NULL || frame == NULL || process->stack_end == NULL ||
      process->fds == NULL || process->children == NULL) {
    return -EINVAL;
  }

  uint64_t child_slot = 0;
  if (!proc_child_find_null(process, &child_slot)) { return -EAGAIN; }

  haddr_t parent_stack_bottom = (haddr_t)process->stack_end;
  haddr_t parent_stack_base = parent_stack_bottom + STACK_SIZE;
  haddr_t parent_frame = (haddr_t)frame;
  if (parent_frame < parent_stack_bottom ||
      parent_frame + sizeof(interrupt_frame_t) > parent_stack_base) {
    return -EINVAL;
  }

  const size_t switch_saved_reg_count = 15;
  size_t switch_frame_size = (switch_saved_reg_count + 1) * sizeof(haddr_t);
  if (sizeof(interrupt_frame_t) + switch_frame_size > STACK_SIZE) {
    return -EINVAL;
  }

  if (process->mem == NULL) { return -EINVAL; }

  vmm_t* new_vmm = vmm_copy(process->mem->vmm);
  if (new_vmm == NULL) {
    hlog_write(HLOG_ERROR,
               "Could not fork new process %s: out of memory.",
               process->name);
    return -ENOMEM;
  }

  proc_t* new_proc = proc_new_shared(process->name, vmm_get_cr3(new_vmm));
  if (new_proc == NULL) {
    vmm_free(new_vmm);
    return -ENOMEM;
  }

  new_proc->mem->vmm = new_vmm;
  new_proc->mem->brk_start = process->mem->brk_start;
  new_proc->mem->brk = process->mem->brk;
  new_proc->mem->stack_start = process->mem->stack_start;
  if (g_kernel.has_fpu) {
    fpu_save(process->fpu);
    fpu_copy(new_proc->fpu, process->fpu);
  }

  haddr_t child_stack_base = (haddr_t)new_proc->stack_end + STACK_SIZE;
  haddr_t child_frame_addr = child_stack_base - sizeof(interrupt_frame_t);
  interrupt_frame_t* child_frame = (interrupt_frame_t*)child_frame_addr;
  memcpy(child_frame, frame, sizeof(interrupt_frame_t));
  child_frame->rax = 0;

  haddr_t switch_rsp = child_frame_addr - switch_frame_size;
  haddr_t* switch_frame = (haddr_t*)switch_rsp;
  memset(switch_frame, 0, switch_frame_size);
  switch_frame[15] = (haddr_t)make_fork_kstack;

  for (uint64_t fd = 0; fd < MAX_FDS; ++fd) {
    new_proc->fds[fd] = process->fds[fd];
    vfs_file_borrow(new_proc->fds[fd]);
  }
  proc_set_cwd(new_proc, process->cwd);

  new_proc->rsp = (void*)switch_rsp;
  new_proc->parent = process;
  new_proc->is_kernel_proc = process->is_kernel_proc;
  new_proc->entry = process->entry;
  new_proc->elf = NULL;
  new_proc->status = PROC_STATUS_READY_TO_RUN;
  process->children[child_slot] = new_proc;
  mp_enqueue_ready(new_proc);
  return new_proc->pid;
}
