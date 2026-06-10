#include <errno.h>
#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <multitask/elf.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

long syscall_execve(const char* pathname,
                    char* const argv[],
                    char* const envp[]) {
  if (pathname == NULL) { return -EINVAL; }

  vfs_file_t* file = NULL;
  vfs_status_t status = vfs_get_file_handle(pathname, VFS_OPEN_READ, &file);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }

  vfs_stat_t* stat = NULL;
  status = vfs_fstat(file, &stat);
  if (status != VFS_STATUS_OK) {
    vfs_close(file);
    return -vfs_status_to_errno(status);
  }

  void* buffer = calloc(1, stat->size);
  if (buffer == NULL) {
    free(stat);
    vfs_close(file);
    return -ENOMEM;
  }

  uint64_t bytes_read = 0;
  status = vfs_read(file, buffer, stat->size, &bytes_read);
  vfs_close(file);
  if (status != VFS_STATUS_OK || bytes_read != stat->size) {
    free(stat);
    free(buffer);
    return status == VFS_STATUS_OK ? -EINVAL : -vfs_status_to_errno(status);
  }

  elf_t* elf = elf_read(buffer, stat->size);
  free(stat);
  if (elf == NULL) {
    free(buffer);
    return -EINVAL;
  }

  long ret = sched_execve(
      g_kernel.current_process, elf, (char*)pathname, strlen(pathname));
  if (ret != 0) {
    elf_free(elf);
    free(buffer);
  }
  return ret;
}
