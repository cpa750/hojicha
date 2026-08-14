#ifndef HOJICHA_MP_PROC_H
#define HOJICHA_MP_PROC_H

#include <cpu/fpu.h>
#include <fs/vfs.h>
#include <haddr.h>
#include <hlog.h>
#include <memory/vmm.h>
#include <mp/wait_queue.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STACK_SIZE   16384  // 4 pages
#define MAX_CHILDREN 256

typedef enum proc_status {
  PROC_STATUS_RUNNING = 0b00000001,
  PROC_STATUS_READY_TO_RUN = 0b00000010,
  PROC_STATUS_SLEEPING_TIMER = 0b00000100,
  PROC_STATUS_BLOCKED = 0b00001000,
  PROC_STATUS_READY_TO_DIE = 0b00010000,
  PROC_STATUS_UNINITIALIZED = 0b11111111,
} proc_status_t;

typedef struct elf elf_t;

typedef struct process_mem process_mem_t;
struct process_mem {
  vmm_t* vmm;
  haddr_t brk_start;
  haddr_t brk;
  haddr_t stack_start;
};

/*
 * The entry point of the process. Must take no parameters and return void.
 */
typedef void (*proc_entry_t)(void);

/*
 * The process control block. This can be created via `sched_kproc_new()` for
 * kernel processes or `sched_uproc_new()` for userland processes.
 */
typedef struct proc proc_t;
struct proc {
  // Begin asm-mapped fields
  void* cr3;
  void* rsp;
  void* rsp0;
  uint8_t status;
  uint8_t is_kernel_proc;
  proc_entry_t entry;
  // End asm-mapped fields

  fpu_t* fpu;
  hlogger_t* logger;
  char* name;
  uint64_t pid;

  proc_t* next;
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
  proc_t** children;
  proc_t* parent;
  wait_queue_t child_waiters;
  wait_queue_t exit_waiters;
  int exit_code;
};

static inline proc_t* proc_get_next(proc_t* p) {
  return p->next;
}

static inline void proc_set_next(proc_t* p, proc_t* next) {
  p->next = next;
}

static inline hlogger_t* proc_get_logger(proc_t* p) {
  return p->logger;
}

static inline char* proc_get_name(proc_t* p) {
  return p->name;
}

static inline uint64_t proc_get_pid(proc_t* p) {
  return p->pid;
}

static inline process_mem_t* proc_get_mem(proc_t* p) {
  return p->mem;
}

static inline void proc_set_elf(proc_t* p, elf_t* elf) {
  p->elf = elf;
}

static inline void* proc_get_cr3(proc_t* p) {
  return p->cr3;
}

static inline vfs_node_t* proc_get_cwd(proc_t* p) {
  if (p == NULL) { return NULL; }
  return p->cwd;
}

static inline void proc_set_cwd(proc_t* p, vfs_node_t* cwd) {
  if (p == NULL) { return; }
  vfs_vnode_borrow(cwd);
  vfs_vnode_release(p->cwd);
  p->cwd = cwd;
}

static inline bool proc_fd_find_null(proc_t* p, uint64_t* idx_out) {
  for (uint16_t i = 0; i < MAX_FDS; ++i) {
    if (p->fds[i] == NULL) {
      if (idx_out != NULL) { *idx_out = i; }
      return true;
    }
  }
  return false;
}

static inline bool proc_child_find_null(proc_t* p, uint64_t* idx_out) {
  if (p == NULL || p->children == NULL) { return false; }

  for (uint64_t i = 1; i < MAX_CHILDREN; ++i) {
    if (p->children[i] == NULL) {
      if (idx_out != NULL) { *idx_out = i; }
      return true;
    }
  }
  return false;
}

static inline vfs_file_t* proc_fd_get(proc_t* p, uint64_t idx) {
  if (idx >= MAX_FDS) { return NULL; }
  return p->fds[idx];
}

static inline void proc_fd_set(proc_t* p, uint64_t idx, vfs_file_t* val) {
  if (idx < MAX_FDS) { p->fds[idx] = val; }
}

#endif  // HOJICHA_MP_PROC_H
