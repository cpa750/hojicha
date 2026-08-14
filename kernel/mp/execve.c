#include <cpu/fpu.h>
#include <errno.h>
#include <haddr.h>
#include <hlog.h>
#include <kernel/elf.h>
#include <kernel/g_kernel.h>
#include <memory/slab.h>
#include <memory/vmm.h>
#include <mp/execve.h>
#include <stdint.h>

extern void load_pd(haddr_t* pd_addr);

long execve_proc(proc_t* process,
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
    slab_free(proc_name);
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

  if (g_kernel.has_fpu) {
    fpu_reset(process->fpu);
    fpu_restore(process->fpu);
  }

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
  slab_free(old_name);
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
