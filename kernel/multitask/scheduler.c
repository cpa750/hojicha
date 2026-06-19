#include <drivers/pit.h>
#include <errno.h>
#include <fs/vfs.h>
#include <haddr.h>
#include <hlog.h>
#include <kernel/g_kernel.h>
#include <memory/vmm.h>
#include <multitask/elf.h>
#include <multitask/scheduler.h>
#include <multitask/wait_queue.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUANTUM_LENGTH        20000000  // 20 ms
#define SCHED_WAITPID_WNOHANG 1

struct sched_state {
  process_block_t* first_ready_to_run;
  process_block_t* last_ready_to_run;
  process_block_t* ready_to_die;
  process_block_t* sleeping;

  uint64_t total_processes_added;
  uint64_t process_count;

  uint64_t idle_switch_timestamp;
  uint64_t quantum_remaining;
  uint64_t tick_interval_ns;
  uint64_t time_elapsed;
  uint64_t time_idle;

  uint64_t irq_lock_count;
  uint64_t switch_lock_count;
  bool switch_lock_flag;

  uint64_t kernel_pid;
};

uint64_t sched_state_get_kernel_pid(sched_state_t* mt) {
  return mt->kernel_pid;
}

typedef struct process_block process_block_t;
struct process_block {
  // Begin asm-mapped fields
  void* cr3;
  void* rsp;
  void* rsp0;
  uint8_t status;
  uint8_t is_kernel_proc;
  proc_entry_t entry;
  // End asm-mapped fields

  hlogger_t* logger;
  char* name;
  uint64_t pid;

  process_block_t* next;
  void* stack_end;

  uint64_t elapsed;
  uint64_t sleep_until;
  uint64_t switch_timestamp;

  process_mem_t* mem;
  elf_t* elf;
  uint64_t argc;
  char** argv;
  char** envp;

  vfs_file_t** fds;
  vfs_node_t* cwd;
  process_block_t** children;
  process_block_t* parent;
  wait_queue_t child_waiters;
  wait_queue_t exit_waiters;
  int exit_code;
};

process_block_t* sched_pb_get_next(process_block_t* p) { return p->next; }
void sched_pb_set_next(process_block_t* p, process_block_t* next) {
  p->next = next;
}
hlogger_t* sched_pb_get_logger(process_block_t* p) { return p->logger; }
char* sched_pb_get_name(process_block_t* p) { return p->name; }
uint64_t sched_pb_get_pid(process_block_t* p) { return p->pid; }
process_mem_t* sched_pb_get_mem(process_block_t* p) { return p->mem; }
void sched_pb_set_elf(process_block_t* p, elf_t* elf) { p->elf = elf; }
vfs_node_t* sched_pb_get_cwd(process_block_t* p) {
  if (p == NULL) { return NULL; }
  return p->cwd;
}
void sched_pb_set_cwd(process_block_t* p, vfs_node_t* cwd) {
  if (p == NULL) { return; }
  vfs_vnode_borrow(cwd);
  vfs_vnode_release(p->cwd);
  p->cwd = cwd;
}
void multitask_pb_dump(process_block_t* p, hlog_level_t log_level) {
  haddr_t vmm_cr3 = 0;
  process_mem_t* mem = sched_pb_get_mem(p);
  if (mem != NULL && mem->vmm != NULL) {
    vmm_cr3 = (haddr_t)vmm_get_cr3(mem->vmm);
  }
  hlog_write(log_level,
             "Process block for \"%s\" (at %x):\n"
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

bool sched_pb_fd_find_null(process_block_t* p, uint64_t* idx_out) {
  for (uint16_t i = 0; i < MAX_FDS; ++i) {
    if (p->fds[i] == NULL) {
      if (idx_out != NULL) { *idx_out = i; }
      return true;
    }
  }
  return false;
}

bool sched_pb_child_find_null(process_block_t* p, uint64_t* idx_out) {
  if (p == NULL || p->children == NULL) { return false; }

  for (uint64_t i = 1; i < MAX_CHILDREN; ++i) {
    if (p->children[i] == NULL) {
      if (idx_out != NULL) { *idx_out = i; }
      return true;
    }
  }
  return false;
}

vfs_file_t* sched_pb_fd_get(process_block_t* p, uint64_t idx) {
  if (idx >= MAX_FDS) { return NULL; }
  return p->fds[idx];
}

void sched_pb_fd_set(process_block_t* p, uint64_t idx, vfs_file_t* val) {
  if (idx < MAX_FDS) { p->fds[idx] = val; }
}

static sched_state_t mt = {0};
static pit_callback_t pit_callback = {0};

extern void switch_to(process_block_t* process, bool is_ctx_switch);
extern void make_fork_kstack(void);
extern void load_pd(haddr_t* pd_addr);

void multitask_switch(process_block_t* process);

void block_process(process_block_t* p, uint8_t reason);
void enqueue_ready_process(process_block_t* process);
process_block_t* find_last_sleep_timestamp_less_than_equal(
    process_block_t* process,
    uint64_t timestamp);
void handle_timer(uint64_t timestamp);
void insert_process_after(process_block_t* process, process_block_t* after);
void mark_proc_range(process_block_t* start,
                     process_block_t* end,
                     proc_status_t status);
process_block_t* new_proc_shared(char* name, void* cr3);
process_mem_t* process_mem_new(vmm_t* vmm);
void process_mem_free(process_mem_t* mem);
char* proc_name_new(const char* name, uint64_t name_len);
void proc_prelude(process_block_t* p);
void proc_strings_free(char** strings);
void process_free(process_block_t* p);
void process_fd_release(vfs_file_t* file);
long process_wait_collect(process_block_t* parent,
                          process_block_t* child,
                          uint64_t child_slot,
                          int* wstatus);
void ready_to_die_remove(process_block_t* target);
void set_last_ready_to_run(sched_state_t* mt,
                           process_block_t* first_ready_to_run);
void remove_proc(process_block_t* p);
void sleep_proc_until(process_block_t* process, uint64_t timestamp);
void terminator(void);
static bool wait_child_find(process_block_t* parent,
                            long pid,
                            process_block_t** child_out,
                            uint64_t* child_slot_out);
void wake_procs_before_timestamp(uint64_t timestamp);

void sched_initialize(void) {
  process_block_t* kernel_process =
      (process_block_t*)malloc(sizeof(process_block_t));
  memset(kernel_process, 0, sizeof(process_block_t));
  haddr_t cr3;
  haddr_t rsp;
  asm volatile("\t movq %%cr3,%0" : "=r"(cr3));
  asm volatile("\t movq %%rsp,%0" : "=r"(rsp));
  kernel_process->cr3 = (void*)cr3;
  kernel_process->rsp = (void*)rsp;
  kernel_process->rsp0 = (void*)rsp;
  kernel_process->next = NULL;
  kernel_process->status = PROC_STATUS_RUNNING;
  kernel_process->is_kernel_proc = true;
  kernel_process->pid = 0;
  kernel_process->name = proc_name_new("hojicha", strlen("hojicha"));
  kernel_process->logger = hlog_new(DEFAULT_HLOG_LEVEL, DEFAULT_HLOG_BUFSIZE);
  kernel_process->mem = process_mem_new(g_kernel.vmm);
  kernel_process->fds = (vfs_file_t**)calloc(1, sizeof(vfs_file_t*) * MAX_FDS);
  kernel_process->children =
      (process_block_t**)calloc(1, sizeof(process_block_t*) * MAX_CHILDREN);
  wait_queue_init(&kernel_process->child_waiters);
  wait_queue_init(&kernel_process->exit_waiters);
  if (kernel_process->name == NULL || kernel_process->logger == NULL ||
      kernel_process->mem == NULL || kernel_process->fds == NULL ||
      kernel_process->children == NULL) {
    free(kernel_process->name);
    if (kernel_process->logger != NULL) {
      hlog_free_logger(kernel_process->logger);
    }
    free(kernel_process->fds);
    free(kernel_process->children);
    process_mem_free(kernel_process->mem);
    free(kernel_process);
    abort();
  }
  mt.kernel_pid = kernel_process->pid;

  mt.first_ready_to_run = NULL;
  mt.last_ready_to_run = NULL;
  mt.ready_to_die = NULL;

  // Initially set to 1 as we don't directly add kernel proc
  mt.total_processes_added = 1;
  mt.process_count = 1;

  mt.tick_interval_ns = pit_state_get_tick_interval_ns(g_kernel.pit);
  g_kernel.sched = &mt;
  g_kernel.current_process = kernel_process;
  pit_callback.callback_func = handle_timer;
  pit_callback.next = NULL;
  pit_register_callback(&pit_callback);

  process_block_t* terminator_task =
      sched_kproc_new("kterminator", terminator, kernel_process->cr3);
  sched_add_proc(terminator_task);
}

process_block_t* sched_kproc_new(char* name, proc_entry_t entry, void* cr3) {
  process_block_t* new_proc = new_proc_shared(name, cr3);
  if (new_proc == NULL) { return NULL; }

  new_proc->is_kernel_proc = true;
  new_proc->entry = entry;
  new_proc->mem->vmm = g_kernel.vmm;
  return new_proc;
}

process_block_t* sched_uproc_new(char* name, elf_t* elf) {
  vmm_t* vmm = vmm_new(PAGE_USER_ACCESIBLE);
  if (vmm == NULL) { return NULL; }

  process_block_t* new_proc = new_proc_shared(name, vmm_get_cr3(vmm));
  if (new_proc == NULL) {
    vmm_free(vmm);
    return NULL;
  }

  for (uint64_t fd = 0; fd < 3; ++fd) {
    vfs_file_t* tty = NULL;
    if (vfs_get_file_handle("/dev/tty0",
                            VFS_OPEN_READ | VFS_OPEN_WRITE,
                            &tty) == VFS_STATUS_OK) {
      sched_pb_fd_set(new_proc, fd, tty);
      continue;
    }

    if (g_kernel.console != NULL) {
      vfs_file_t* console = NULL;
      if (vfs_get_file_handle("/dev/console", VFS_OPEN_WRITE, &console) ==
          VFS_STATUS_OK) {
        sched_pb_fd_set(new_proc, fd, console);
      }
    }
  }

  new_proc->is_kernel_proc = false;
  new_proc->elf = elf;
  new_proc->mem->vmm = vmm;
  sched_pb_set_cwd(new_proc, sched_pb_get_cwd(g_kernel.current_process));
  return new_proc;
}

long sched_execve(process_block_t* process,
                  elf_t* elf,
                  char* name,
                  uint64_t name_len,
                  uint64_t argc,
                  char** argv,
                  char** envp) {
  if (process == NULL || process != g_kernel.current_process || elf == NULL ||
      process->fds == NULL || process->mem == NULL) {
    proc_strings_free(argv);
    proc_strings_free(envp);
    return -EINVAL;
  }

  hlogger_t* logger = hlog_new(DEFAULT_HLOG_LEVEL, DEFAULT_HLOG_BUFSIZE);
  vmm_t* vmm = vmm_new(PAGE_USER_ACCESIBLE);
  char* proc_name = proc_name_new(name, name_len);
  if (logger == NULL || vmm == NULL || proc_name == NULL) {
    if (logger != NULL) { hlog_free_logger(logger); }
    if (vmm != NULL) { vmm_free(vmm); }
    free(proc_name);
    proc_strings_free(argv);
    proc_strings_free(envp);
    return -ENOMEM;
  }

  for (uint64_t fd = 0; fd < MAX_FDS; ++fd) {
    vfs_file_t* file = process->fds[fd];
    if (file == NULL) { continue; }

    if (file->flags & VFS_OPEN_CLOEXEC) {
      process->fds[fd] = NULL;
      vfs_close(file);
    }
  }

  char* old_name = process->name;
  hlogger_t* old_logger = process->logger;
  vmm_t* old_vmm = process->mem->vmm;
  elf_t* old_elf = process->elf;
  char** old_argv = process->argv;
  char** old_envp = process->envp;

  process->name = proc_name;
  process->logger = logger;
  process->mem->vmm = vmm;
  process->mem->brk_start = 0;
  process->mem->brk = 0;
  process->mem->stack_start = 0;
  process->cr3 = vmm_get_cr3(vmm);
  process->elf = elf;
  process->argc = argc;
  process->argv = argv;
  process->envp = envp;
  process->is_kernel_proc = false;

  load_pd(process->cr3);
  if (old_vmm != NULL && old_vmm != g_kernel.vmm) { vmm_free(old_vmm); }
  free(old_name);
  if (old_logger != NULL) { hlog_free_logger(old_logger); }
  if (old_elf != NULL && old_elf != elf) { elf_free(old_elf); }
  proc_strings_free(old_argv);
  proc_strings_free(old_envp);

  elf_launch(
      process->elf, process->mem, process->argc, process->argv, process->envp);
  proc_strings_free(process->argv);
  proc_strings_free(process->envp);
  process->argc = 0;
  process->argv = NULL;
  process->envp = NULL;
  return -EINVAL;
}

long sched_fork(process_block_t* process, interrupt_frame_t* frame) {
  if (process == NULL || frame == NULL || process->stack_end == NULL ||
      process->fds == NULL || process->children == NULL) {
    return -EINVAL;
  }

  uint64_t child_slot = 0;
  if (!sched_pb_child_find_null(process, &child_slot)) { return -EAGAIN; }

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

  process_block_t* new_proc =
      new_proc_shared(process->name, vmm_get_cr3(new_vmm));
  if (new_proc == NULL) {
    vmm_free(new_vmm);
    return -ENOMEM;
  }

  new_proc->mem->vmm = new_vmm;
  new_proc->mem->brk_start = process->mem->brk_start;
  new_proc->mem->brk = process->mem->brk;
  new_proc->mem->stack_start = process->mem->stack_start;

  haddr_t child_stack_base = (haddr_t)new_proc->stack_end + STACK_SIZE;
  haddr_t child_frame_addr = child_stack_base - sizeof(interrupt_frame_t);
  interrupt_frame_t* child_frame = (interrupt_frame_t*)child_frame_addr;
  memcpy(child_frame, frame, sizeof(interrupt_frame_t));
  child_frame->rax = 0;

  haddr_t switch_rsp = child_frame_addr - switch_frame_size;
  haddr_t* switch_frame = (haddr_t*)switch_rsp;
  memset(switch_frame, 0, switch_frame_size);
  // switch_to pops this dummy kernel context and ret jumps to the fork
  // epilogue. make_fork_kstack then restores the copied interrupt frame above
  // it.
  switch_frame[15] = (haddr_t)make_fork_kstack;

  for (uint64_t fd = 0; fd < MAX_FDS; ++fd) {
    new_proc->fds[fd] = process->fds[fd];
    vfs_file_borrow(new_proc->fds[fd]);
  }
  sched_pb_set_cwd(new_proc, process->cwd);

  new_proc->rsp = (void*)switch_rsp;
  new_proc->parent = process;
  new_proc->is_kernel_proc = process->is_kernel_proc;
  new_proc->entry = process->entry;
  new_proc->elf = NULL;
  new_proc->status = PROC_STATUS_READY_TO_RUN;
  process->children[child_slot] = new_proc;
  enqueue_ready_process(new_proc);
  return new_proc->pid;
}

long sched_waitpid(process_block_t* process,
                   long pid,
                   int* wstatus,
                   int options) {
  if (process == NULL || process->children == NULL) { return -ECHILD; }
  if ((options & ~SCHED_WAITPID_WNOHANG) != 0 || pid == 0 || pid < -1) {
    return -EINVAL;
  }

  sched_postpone();

  process_block_t* child = NULL;
  uint64_t child_slot = 0;
  bool has_child = wait_child_find(process, pid, &child, &child_slot);

  if (!has_child) {
    sched_resume();
    return -ECHILD;
  }

  if (child->status == PROC_STATUS_READY_TO_DIE) {
    long ret = process_wait_collect(process, child, child_slot, wstatus);
    sched_resume();
    return ret;
  }

  if ((options & SCHED_WAITPID_WNOHANG) != 0) {
    sched_resume();
    return 0;
  }

  if (pid == -1) {
    wait_queue_sleep_postponed(&process->child_waiters);
    has_child = wait_child_find(process, pid, &child, &child_slot);
  } else {
    wait_queue_sleep_postponed(&child->exit_waiters);
  }

  if (!has_child || child == NULL ||
      child->status != PROC_STATUS_READY_TO_DIE) {
    sched_resume();
    return -ECHILD;
  }

  long ret = process_wait_collect(process, child, child_slot, wstatus);
  sched_resume();
  return ret;
}

void sched_add_proc(process_block_t* process) {
  sched_postpone();
  enqueue_ready_process(process);
  sched_resume();
}

void schedule_advance(void) {
  // TODO: refactor this mess of a function
  if (g_kernel.sched->switch_lock_count > 0) {
    g_kernel.sched->switch_lock_flag = true;
    return;
  }

  g_kernel.sched->time_elapsed = pit_get_ns_elapsed_since_init(g_kernel.pit);
  if (g_kernel.current_process != NULL) {
    g_kernel.current_process->elapsed +=
        g_kernel.sched->time_elapsed -
        g_kernel.current_process->switch_timestamp;
  } else {
    g_kernel.sched->time_idle +=
        g_kernel.sched->time_elapsed - g_kernel.sched->idle_switch_timestamp;
  }

  if (g_kernel.sched->first_ready_to_run != NULL) {
    if (g_kernel.current_process != NULL &&
        g_kernel.current_process->status == PROC_STATUS_RUNNING) {
      g_kernel.current_process->status = PROC_STATUS_READY_TO_RUN;

      // We only want to place the old process in the ready-to-run queue if
      // it's being pre-empted. Otherwise, (for example, sleeping) it belongs
      // in a difference queue handled elsewhere.
      if (g_kernel.sched->last_ready_to_run == NULL) {
        g_kernel.sched->last_ready_to_run = g_kernel.sched->first_ready_to_run;
      }
      g_kernel.sched->last_ready_to_run->next = g_kernel.current_process;
      g_kernel.sched->last_ready_to_run =
          g_kernel.sched->last_ready_to_run->next;
      if (g_kernel.sched->first_ready_to_run !=
          g_kernel.sched->last_ready_to_run) {
        g_kernel.sched->last_ready_to_run->next = NULL;
      }
    }

    g_kernel.sched->quantum_remaining = QUANTUM_LENGTH;
    process_block_t* next = g_kernel.sched->first_ready_to_run;
    next->switch_timestamp = g_kernel.sched->time_elapsed;
    g_kernel.sched->first_ready_to_run =
        g_kernel.sched->first_ready_to_run->next;
    if (next == g_kernel.sched->last_ready_to_run) {
      g_kernel.sched->last_ready_to_run = NULL;
    }

    // TODO: what happens if we sleep the only available process?
    // g_kernel.current_process and its status is updated inside switch_to()
    multitask_switch(next);
  } else if (g_kernel.current_process->status != PROC_STATUS_RUNNING) {
    process_block_t* proc = g_kernel.current_process;
    g_kernel.current_process = NULL;
    g_kernel.sched->idle_switch_timestamp = g_kernel.sched->time_elapsed;

    do {
      asm volatile("sti");
      asm volatile("hlt");
      asm volatile("cli");
    } while (g_kernel.sched->first_ready_to_run == NULL);

    g_kernel.sched->quantum_remaining = 0;
    g_kernel.current_process = proc;
    proc->switch_timestamp = g_kernel.sched->time_elapsed;
    proc = g_kernel.sched->first_ready_to_run;
    g_kernel.sched->first_ready_to_run =
        g_kernel.sched->first_ready_to_run->next;
    if (proc == g_kernel.sched->last_ready_to_run) {
      g_kernel.sched->last_ready_to_run = NULL;
    }
    multitask_switch(proc);
  }
}

void sched_yield(void) {
  sched_lock();
  schedule_advance();
  sched_unlock();
}

void multitask_switch(process_block_t* process) {
  asm volatile("cli");
  uint64_t cs = 0;
  asm volatile("\t movq %%cs,%0" : "=r"(cs));
  uint8_t cpl = cs & 0b11;
  bool is_ctx_switch = (!cpl && !process->is_kernel_proc) ||
                       (cpl == 3 && process->is_kernel_proc);
  hlog_write(HLOG_DEBUG,
             "switching to PID %d from %d",
             process->pid,
             g_kernel.current_process->pid);
  switch_to(process, is_ctx_switch);
  asm volatile("sti");
}

void sched_lock(void) {
  // TODO: this will need more fleshing out for multi-core support
  asm volatile("cli");
  g_kernel.sched->irq_lock_count++;
}

void sched_unlock(void) {
  if (g_kernel.sched->irq_lock_count > 0) { g_kernel.sched->irq_lock_count--; }
  if (g_kernel.sched->irq_lock_count == 0) { asm volatile("sti"); }
}

void sched_postpone(void) {
  // TODO: this will need more fleshing out for multi-core support
  asm volatile("cli");
  g_kernel.sched->irq_lock_count++;
  g_kernel.sched->switch_lock_count++;
}

void sched_resume(void) {
  if (g_kernel.sched->switch_lock_count > 0) {
    g_kernel.sched->switch_lock_count--;
  }
  if (g_kernel.sched->switch_lock_count == 0 &&
      g_kernel.sched->switch_lock_flag) {
    g_kernel.sched->switch_lock_flag = false;
    schedule_advance();
  }
  if (g_kernel.sched->irq_lock_count > 0) { g_kernel.sched->irq_lock_count--; }
  if (g_kernel.sched->irq_lock_count == 0) { asm volatile("sti"); }
}

void sched_current_block(uint8_t reason) {
  block_process(g_kernel.current_process, reason);
}

void sched_proc_unblock(process_block_t* process) {
  if (process == NULL) { return; }

  sched_lock();

  process->status = PROC_STATUS_READY_TO_RUN;
  process->next = NULL;

  if (g_kernel.sched->first_ready_to_run == NULL) {
    g_kernel.sched->first_ready_to_run = process;
    g_kernel.sched->last_ready_to_run = process;
  } else {
    if (g_kernel.sched->last_ready_to_run == NULL) {
      set_last_ready_to_run(g_kernel.sched, g_kernel.sched->first_ready_to_run);
    }
    g_kernel.sched->last_ready_to_run->next = process;
    g_kernel.sched->last_ready_to_run = process;
  }

  sched_unlock();
}

void sched_current_sleep(uint64_t s) {
  sched_current_sleep_ns(s * 1000000000ULL);
}

void sched_current_sleep_ns(uint64_t ns) {
  sleep_proc_until(g_kernel.current_process,
                   pit_get_ns_elapsed_since_init(g_kernel.pit) + ns);
}

void sched_proc_terminate(process_block_t* p) { sched_proc_exit(p, 0); }

void sched_proc_exit(process_block_t* p, int code) {
  if (p == NULL) { return; }
  sched_postpone();

  sched_lock();
  if (p->status == PROC_STATUS_READY_TO_DIE) {
    sched_unlock();
    sched_resume();
    return;
  }
  p->exit_code = code;
  if (p != g_kernel.current_process) { remove_proc(p); }
  p->next = g_kernel.sched->ready_to_die;
  g_kernel.sched->ready_to_die = p;
  sched_unlock();

  block_process(p, PROC_STATUS_READY_TO_DIE);
  if (p->parent != NULL) {
    wait_queue_wake_all(&p->exit_waiters);
    wait_queue_wake_all(&p->parent->child_waiters);
  }
  sched_resume();
}

void* sched_pb_get_cr3(process_block_t* p) { return p->cr3; }

void block_process(process_block_t* p, uint8_t reason) {
  sched_lock();
  p->status = reason;
  schedule_advance();
  sched_unlock();
}

void enqueue_ready_process(process_block_t* process) {
  process->next = NULL;
  if (g_kernel.sched->first_ready_to_run == NULL) {
    g_kernel.sched->first_ready_to_run = process;
    g_kernel.sched->last_ready_to_run = process;
    return;
  }

  if (g_kernel.sched->last_ready_to_run == NULL) {
    set_last_ready_to_run(g_kernel.sched, g_kernel.sched->first_ready_to_run);
  }
  g_kernel.sched->last_ready_to_run->next = process;
  g_kernel.sched->last_ready_to_run = process;
}

/*
 * Finds the last process needing to be woken.
 */
process_block_t* find_last_sleep_timestamp_less_than_equal(
    process_block_t* process,
    uint64_t timestamp) {
  if (process->sleep_until > timestamp) { return NULL; }
  process_block_t* ret = NULL;
  for (process_block_t* curr = process; curr != NULL; curr = curr->next) {
    if (curr->sleep_until <= timestamp) {
      ret = curr;
    } else {
      break;
    }
  }
  return ret;
}

void handle_timer(uint64_t timestamp) {
  // TODO: figure out why calls to printf() that are preempted break
  // framebuffer line scrolling
  sched_postpone();
  wake_procs_before_timestamp(timestamp);
  if (g_kernel.sched->quantum_remaining != 0) {
    if (g_kernel.sched->quantum_remaining <= g_kernel.sched->tick_interval_ns) {
      sched_lock();
      schedule_advance();
      sched_unlock();
    } else {
      g_kernel.sched->quantum_remaining -= g_kernel.sched->tick_interval_ns;
    }
  }
  sched_resume();
}

void insert_process_after(process_block_t* process, process_block_t* after) {
  if (after == NULL) { return; }

  process->next = after->next;
  after->next = process;
}

process_block_t* new_proc_shared(char* name, void* cr3) {
  process_block_t* new_proc =
      (process_block_t*)calloc(1, sizeof(process_block_t));
  vfs_file_t** fds = (vfs_file_t**)calloc(1, sizeof(vfs_file_t*) * MAX_FDS);
  process_block_t** children =
      (process_block_t**)calloc(1, sizeof(process_block_t*) * MAX_CHILDREN);
  uint8_t* stack_end = (uint8_t*)calloc(1, STACK_SIZE);
  hlogger_t* logger = hlog_new(DEFAULT_HLOG_LEVEL, DEFAULT_HLOG_BUFSIZE);
  process_mem_t* mem = process_mem_new(NULL);
  uint64_t pid = g_kernel.sched->total_processes_added + 1;
  char pid_name[21] = {0};
  if (name == NULL) {
    itoa(pid, pid_name, 10);
    name = pid_name;
  }
  char* proc_name = proc_name_new(name, strlen(name));
  if (new_proc == NULL || mem == NULL || fds == NULL || children == NULL ||
      stack_end == NULL || logger == NULL || proc_name == NULL) {
    free(new_proc);
    free(fds);
    free(children);
    free(stack_end);
    process_mem_free(mem);
    free(proc_name);
    if (logger != NULL) { hlog_free_logger(logger); }
    hlog_write(HLOG_ERROR,
               "Could not create new process %s: out of memory.",
               name == NULL ? "" : name);
    return NULL;
  }

  new_proc->pid = pid;
  new_proc->cr3 = cr3;
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
  char* ret = (char*)calloc(name_len + 1, sizeof(char));
  if (ret == NULL) { return NULL; }
  memcpy(ret, name, name_len);
  ret[name_len] = '\0';
  return ret;
}

process_mem_t* process_mem_new(vmm_t* vmm) {
  process_mem_t* mem = calloc(1, sizeof(process_mem_t));
  if (mem == NULL) { return NULL; }
  mem->vmm = vmm;
  return mem;
}

void process_mem_free(process_mem_t* mem) {
  if (mem == NULL) { return; }
  if (mem->vmm != NULL && mem->vmm != g_kernel.vmm) { vmm_free(mem->vmm); }
  free(mem);
}

void proc_strings_free(char** strings) {
  if (strings == NULL) { return; }

  for (uint64_t i = 0; strings[i] != NULL; ++i) { free(strings[i]); }
  free(strings);
}

void proc_prelude(process_block_t* p) {
  if (p == NULL) { return; }
  if (p->status == PROC_STATUS_UNINITIALIZED) {
    p->status = PROC_STATUS_RUNNING;
    sched_unlock();
  }
  // TODO wire in userspace switch, user stack and elf binary launch here
  if (p->is_kernel_proc) {
    p->entry();
  } else {
    elf_launch(p->elf, p->mem, p->argc, p->argv, p->envp);
  }
  sched_proc_terminate(p);
}

void process_free(process_block_t* p) {
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
    free(p->children);
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
    free(p->fds);
  }
  free(p->name);
  proc_strings_free(p->argv);
  proc_strings_free(p->envp);
  sched_pb_set_cwd(p, NULL);
  process_mem_free(p->mem);
  free(p->stack_end);
  free(p);
}

void process_fd_release(vfs_file_t* file) {
  if (file == NULL) { return; }

  if (file->refcount > 1) {
    vfs_file_release(file);
    return;
  }

  vfs_close(file);
}

long process_wait_collect(process_block_t* parent,
                          process_block_t* child,
                          uint64_t child_slot,
                          int* wstatus) {
  if (parent == NULL || child == NULL || parent->children == NULL) {
    return -ECHILD;
  }
  if (child->status != PROC_STATUS_READY_TO_DIE) { return -ECHILD; }

  long child_pid = child->pid;
  if (wstatus != NULL) { *wstatus = (child->exit_code & 0xFF) << 8; }

  parent->children[child_slot] = NULL;
  child->parent = NULL;
  ready_to_die_remove(child);
  process_free(child);
  return child_pid;
}

void ready_to_die_remove(process_block_t* target) {
  if (target == NULL) { return; }

  process_block_t* prev = NULL;
  for (process_block_t* p = g_kernel.sched->ready_to_die; p != NULL;
       p = p->next) {
    if (p != target) {
      prev = p;
      continue;
    }

    if (prev == NULL) {
      g_kernel.sched->ready_to_die = p->next;
    } else {
      prev->next = p->next;
    }
    p->next = NULL;
    return;
  }
}

void remove_proc(process_block_t* p) {
  // TODO: tidy this up
  process_block_t* head;
  bool is_ready_queue = false;
  switch (p->status) {
    case PROC_STATUS_READY_TO_RUN:
    case PROC_STATUS_UNINITIALIZED:
      head = g_kernel.sched->first_ready_to_run;
      is_ready_queue = true;
      break;
    case PROC_STATUS_SLEEPING_TIMER:
      head = g_kernel.sched->sleeping;
      break;
    default:
      return;
  }

  if (p == head) {
    switch (p->status) {
      case PROC_STATUS_READY_TO_RUN:
      case PROC_STATUS_UNINITIALIZED:
        g_kernel.sched->first_ready_to_run = p->next;
        if (g_kernel.sched->last_ready_to_run == p) {
          g_kernel.sched->last_ready_to_run =
              g_kernel.sched->first_ready_to_run;
        }
        break;
      case PROC_STATUS_SLEEPING_TIMER:
        g_kernel.sched->sleeping = p->next;
        break;
      default:
        return;
    }
  } else {
    process_block_t* last = head;
    process_block_t* proc;
    for (proc = head; proc != p && proc != NULL; proc = proc->next) {
      last = proc;
    }
    if (proc != NULL) {
      last->next = proc->next;
      if (is_ready_queue && g_kernel.sched->last_ready_to_run == proc) {
        g_kernel.sched->last_ready_to_run = last;
      }
      proc->next = NULL;
    }
  }
}

void set_last_ready_to_run(sched_state_t* mt,
                           process_block_t* first_ready_to_run) {
  if (first_ready_to_run->next == NULL) {
    mt->last_ready_to_run = first_ready_to_run;
    return;
  }
  set_last_ready_to_run(mt, first_ready_to_run->next);
}

void sleep_proc_until(process_block_t* process, uint64_t timestamp) {
  sched_postpone();
  process->sleep_until = timestamp;
  process->status = PROC_STATUS_SLEEPING_TIMER;

  // TODO rewrite this to use `multitask_block()`

  if (g_kernel.sched->sleeping == NULL) {
    process->next = NULL;
    g_kernel.sched->sleeping = process;
    sched_lock();
    schedule_advance();
    sched_unlock();
    sched_resume();
    return;
  }

  process_block_t* last = find_last_sleep_timestamp_less_than_equal(
      g_kernel.sched->sleeping, timestamp);
  if (last == NULL) {
    process->next = g_kernel.sched->sleeping;
    g_kernel.sched->sleeping = process;
  }
  insert_process_after(process, last);

  sched_lock();
  schedule_advance();
  sched_unlock();
  sched_resume();
}

/*
 * The task to terminate all tasks.
 */
void terminator(void) {
  while (true) {
    if (g_kernel.sched->ready_to_die == NULL) {
      // TODO: improve this so it immediately yields with `multitask_block()`
      sched_lock();
      schedule_advance();
      sched_unlock();
    } else {
      sched_postpone();
      process_block_t* waiting_for_parent = NULL;
      process_block_t* next;
      bool freed_process = false;
      for (process_block_t* p = g_kernel.sched->ready_to_die; p != NULL;) {
        next = p->next;
        p->next = NULL;
        if (p->parent != NULL) {
          p->next = waiting_for_parent;
          waiting_for_parent = p;
        } else {
          // TODO: we ideally need to hand the logs committing off to a
          // dedicated task
          process_free(p);
          freed_process = true;
        }
        p = next;
      }
      g_kernel.sched->ready_to_die = waiting_for_parent;
      sched_resume();
      if (!freed_process) { sched_yield(); }
    }
  }
}

static bool wait_child_find(process_block_t* parent,
                            long pid,
                            process_block_t** child_out,
                            uint64_t* child_slot_out) {
  if (parent == NULL || parent->children == NULL) { return false; }

  process_block_t* first_matching_child = NULL;
  uint64_t first_matching_child_slot = 0;
  for (uint64_t child_slot = 1; child_slot < MAX_CHILDREN; ++child_slot) {
    process_block_t* child = parent->children[child_slot];
    if (child == NULL) { continue; }
    if (pid != -1 && child->pid != (uint64_t)pid) { continue; }

    if (child->status == PROC_STATUS_READY_TO_DIE) {
      if (child_out != NULL) { *child_out = child; }
      if (child_slot_out != NULL) { *child_slot_out = child_slot; }
      return true;
    }

    if (first_matching_child == NULL) {
      first_matching_child = child;
      first_matching_child_slot = child_slot;
    }
  }

  if (first_matching_child == NULL) { return false; }

  if (child_out != NULL) { *child_out = first_matching_child; }
  if (child_slot_out != NULL) { *child_slot_out = first_matching_child_slot; }
  return true;
}

void wake_procs_before_timestamp(uint64_t timestamp) {
  if (g_kernel.sched->sleeping == NULL ||
      g_kernel.sched->sleeping->sleep_until > timestamp) {
    return;
  }

  process_block_t* last_to_wake = NULL;
  for (process_block_t* curr = g_kernel.sched->sleeping; curr != NULL;
       curr = curr->next) {
    if (curr->sleep_until <= timestamp) {
      curr->status = PROC_STATUS_READY_TO_RUN;
      last_to_wake = curr;
    } else {
      break;
    }
  }

  if (g_kernel.sched->first_ready_to_run == NULL) {
    g_kernel.sched->first_ready_to_run = g_kernel.sched->sleeping;
    g_kernel.sched->last_ready_to_run = last_to_wake;
  } else {
    g_kernel.sched->last_ready_to_run->next = g_kernel.sched->sleeping;
    g_kernel.sched->last_ready_to_run = last_to_wake;
  }

  g_kernel.sched->sleeping = last_to_wake->next;
  g_kernel.sched->last_ready_to_run->next = NULL;
}
