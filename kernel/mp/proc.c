#include <cpu/fpu.h>
#include <errno.h>
#include <fs/vfs.h>
#include <haddr.h>
#include <hlog.h>
#include <kernel/elf.h>
#include <kernel/g_kernel.h>
#include <memory/slab.h>
#include <memory/vmm.h>
#include <mp/proc.h>
#include <mp/scheduler.h>
#include <mp/wait_queue.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mp_utils.h"

static void process_fd_release(vfs_file_t* file);

proc_t* proc_new_kernel(char* name, proc_entry_t entry, void* cr3) {
  proc_t* new_proc = proc_new_shared(name, cr3);
  if (new_proc == NULL) { return NULL; }

  new_proc->is_kernel_proc = true;
  new_proc->entry = entry;
  new_proc->mem->vmm = g_kernel.vmm;
  return new_proc;
}

proc_t* proc_new_user(char* name, elf_t* elf) {
  vmm_t* vmm = vmm_new(PAGE_USER_ACCESIBLE);
  if (vmm == NULL) { return NULL; }

  proc_t* new_proc = proc_new_shared(name, vmm_get_cr3(vmm));
  if (new_proc == NULL) {
    vmm_free(vmm);
    return NULL;
  }

  for (uint64_t fd = 0; fd < 3; ++fd) {
    vfs_file_t* tty = NULL;
    if (vfs_get_file_handle("/dev/tty0",
                            VFS_OPEN_READ | VFS_OPEN_WRITE,
                            &tty) == VFS_STATUS_OK) {
      proc_fd_set(new_proc, fd, tty);
      continue;
    }

    if (g_kernel.console != NULL) {
      vfs_file_t* console = NULL;
      if (vfs_get_file_handle("/dev/console", VFS_OPEN_WRITE, &console) ==
          VFS_STATUS_OK) {
        proc_fd_set(new_proc, fd, console);
      }
    }
  }

  new_proc->is_kernel_proc = false;
  new_proc->elf = elf;
  new_proc->mem->vmm = vmm;
  proc_set_cwd(new_proc, proc_get_cwd(g_kernel.current_process));
  return new_proc;
}

void proc_dump(proc_t* p, hlog_level_t log_level) {
  haddr_t vmm_cr3 = 0;
  process_mem_t* mem = proc_get_mem(p);
  if (mem != NULL && mem->vmm != NULL) {
    vmm_cr3 = (haddr_t)vmm_get_cr3(mem->vmm);
  }
  hlog_write(log_level,
             "Proc \"%s\" (at %x):\n"
             "CR3:\t\t\t%x\n"
             "RSP0:\t\t\t%x\n"
             "RSP:\t\t\t%x\n"
             "Status:\t\t\t%d\n"
             "Is kernel:\t\t%d\n"
             "Entry:\t\t\t%x\n"
             "PID:\t\t\t%d\n"
             "Next:\t\t\t%x\n"
             "Stack end:\t\t%x\n"
             "VMM:\t\t\t%x\n"
             "VMM->pml4_phy:\t%x\n"
             "brk_start:\t\t%x\n"
             "brk:\t\t\t%x\n"
             "stack_start:\t%x\n"
             "elf:\t\t\t%x\n",
             p->name,
             (haddr_t)p,
             (haddr_t)p->cr3,
             (haddr_t)p->rsp0,
             (haddr_t)p->rsp,
             (haddr_t)p->status,
             (haddr_t)p->is_kernel_proc,
             (haddr_t)p->entry,
             (haddr_t)p->pid,
             (haddr_t)p->next,
             (haddr_t)p->stack_end,
             mem == NULL ? 0 : (haddr_t)mem->vmm,
             vmm_cr3,
             mem == NULL ? 0 : mem->brk_start,
             mem == NULL ? 0 : mem->brk,
             mem == NULL ? 0 : mem->stack_start,
             (haddr_t)p->elf);
}

proc_t* proc_new_shared(char* name, void* cr3) {
  proc_t* new_proc = (proc_t*)slab_calloc(sizeof(proc_t));
  vfs_file_t** fds = (vfs_file_t**)slab_calloc(sizeof(vfs_file_t*) * MAX_FDS);
  proc_t** children =
      (proc_t**)slab_calloc(sizeof(proc_t*) * MAX_CHILDREN);
  uint8_t* stack_end = (uint8_t*)slab_calloc(STACK_SIZE);
  hlogger_t* logger = hlog_new(DEFAULT_HLOG_LEVEL, DEFAULT_HLOG_BUFSIZE);
  process_mem_t* mem = process_mem_new(NULL);
  fpu_t* fpu = g_kernel.has_fpu ? fpu_new() : NULL;
  uint64_t pid = g_kernel.sched->total_processes_added + 1;
  char pid_name[21] = {0};
  if (name == NULL) {
    itoa(pid, pid_name, 10);
    name = pid_name;
  }
  char* proc_name = proc_name_new(name, strlen(name));
  if (new_proc == NULL || mem == NULL || fds == NULL || children == NULL ||
      stack_end == NULL || logger == NULL || proc_name == NULL ||
      (g_kernel.has_fpu && fpu == NULL)) {
    slab_free(new_proc);
    slab_free(fds);
    slab_free(children);
    slab_free(stack_end);
    fpu_free(fpu);
    process_mem_free(mem);
    slab_free(proc_name);
    if (logger != NULL) { hlog_free_logger(logger); }
    hlog_write(HLOG_ERROR,
               "Could not create new process %s: out of memory.",
               name == NULL ? "" : name);
    return NULL;
  }

  new_proc->pid = pid;
  new_proc->cr3 = cr3;
  new_proc->fpu = fpu;
  new_proc->stack_end = stack_end;
  new_proc->mem = mem;
  haddr_t stack_base = (haddr_t)(stack_end + STACK_SIZE);
  new_proc->rsp0 = (void*)stack_base;
  stack_base &= ~0xFULL;
  haddr_t return_slot = stack_base - 16;
  haddr_t switch_rsp = return_slot - (15 * sizeof(haddr_t));
  haddr_t* switch_frame = (haddr_t*)switch_rsp;
  memset(switch_frame, 0, (15 + 1) * sizeof(haddr_t));
  switch_frame[9] = (haddr_t)new_proc;
  switch_frame[15] = (haddr_t)proc_prelude;
  new_proc->fds = fds;
  new_proc->children = children;
  wait_queue_init(&new_proc->child_waiters);
  wait_queue_init(&new_proc->exit_waiters);

  new_proc->name = proc_name;

  ++(g_kernel.sched->total_processes_added);
  ++(g_kernel.sched->process_count);
  new_proc->logger = logger;

  new_proc->rsp = (void*)switch_rsp;
  new_proc->status = PROC_STATUS_UNINITIALIZED;
  return new_proc;
}

char* proc_name_new(const char* name, uint64_t name_len) {
  if (name == NULL) { return NULL; }
  char* ret = (char*)slab_calloc(name_len + 1);
  if (ret == NULL) { return NULL; }
  memcpy(ret, name, name_len);
  ret[name_len] = '\0';
  return ret;
}

process_mem_t* process_mem_new(vmm_t* vmm) {
  process_mem_t* mem = slab_calloc(sizeof(process_mem_t));
  if (mem == NULL) { return NULL; }
  mem->vmm = vmm;
  return mem;
}

void process_mem_free(process_mem_t* mem) {
  if (mem == NULL) { return; }
  if (mem->vmm != NULL && mem->vmm != g_kernel.vmm) { vmm_free(mem->vmm); }
  slab_free(mem);
}

void proc_strings_free(char** strings) {
  if (strings == NULL) { return; }

  for (uint64_t i = 0; strings[i] != NULL; ++i) { free(strings[i]); }
  free(strings);
}

void proc_prelude(proc_t* p) {
  if (p == NULL) { return; }
  if (p->status == PROC_STATUS_UNINITIALIZED) {
    p->status = PROC_STATUS_RUNNING;
    sched_unlock();
  }
  if (p->is_kernel_proc) {
    p->entry();
  } else {
    elf_launch(p->elf, p->mem, p->argc, p->argv, p->envp);
  }
  proc_terminate(p);
}

void proc_free(proc_t* p) {
  if (p == NULL) { return; }

  if (p->parent != NULL && p->parent->children != NULL) {
    for (uint64_t child_slot = 1; child_slot < MAX_CHILDREN; ++child_slot) {
      if (p->parent->children[child_slot] == p) {
        p->parent->children[child_slot] = NULL;
        break;
      }
    }
  }

  if (p->children != NULL) {
    for (uint64_t child_slot = 1; child_slot < MAX_CHILDREN; ++child_slot) {
      if (p->children[child_slot] != NULL) {
        p->children[child_slot]->parent = NULL;
      }
    }
    slab_free(p->children);
  }
  hlog_free_logger(p->logger);
  if (p->fds != NULL) {
    for (uint64_t fd = 0; fd < MAX_FDS; ++fd) {
      if (p->fds[fd] != NULL) {
        vfs_file_t* file = p->fds[fd];
        p->fds[fd] = NULL;
        process_fd_release(file);
      }
    }
    slab_free(p->fds);
  }
  slab_free(p->name);
  proc_strings_free(p->argv);
  proc_strings_free(p->envp);
  proc_set_cwd(p, NULL);
  fpu_free(p->fpu);
  process_mem_free(p->mem);
  slab_free(p->stack_end);
  slab_free(p);
}

void proc_terminate(proc_t* p) { proc_exit(p, 0); }

void proc_exit(proc_t* p, int code) {
  if (p == NULL) { return; }
  sched_postpone();

  sched_lock();
  if (p->status == PROC_STATUS_READY_TO_DIE) {
    sched_unlock();
    sched_resume();
    return;
  }
  p->exit_code = code;
  if (p != g_kernel.current_process) { mp_remove_proc(p); }
  p->next = g_kernel.sched->ready_to_die;
  g_kernel.sched->ready_to_die = p;
  sched_unlock();

  mp_block_proc(p, PROC_STATUS_READY_TO_DIE);
  if (p->parent != NULL) {
    wait_queue_wake_all(&p->exit_waiters);
    wait_queue_wake_all(&p->parent->child_waiters);
  }
  sched_resume();
}

static void process_fd_release(vfs_file_t* file) {
  if (file == NULL) { return; }

  if (file->refcount > 1) {
    vfs_file_release(file);
    return;
  }

  vfs_close(file);
}
