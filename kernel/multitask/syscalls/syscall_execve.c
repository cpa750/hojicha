#include <errno.h>
#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <multitask/elf.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

long syscall_execve(const char* pathname,
                    char* const argv[],
                    char* const envp[]) {
  if (pathname == NULL) { return -EINVAL; }
  if (!syscall_is_uaddr(pathname, SYSCALL_USER_STRING_MAX)) {
    return -EINVAL;
  }

  char* path_copy =
      syscall_utok_strcpy(pathname, SYSCALL_USER_STRING_MAX);
  if (path_copy == NULL) { return -ENOMEM; }

  vfs_file_t* file = NULL;
  vfs_status_t status = vfs_get_file_handle(path_copy, VFS_OPEN_READ, &file);
  if (status != VFS_STATUS_OK) {
    free(path_copy);
    return -vfs_status_to_errno(status);
  }

  vfs_stat_t* stat = NULL;
  status = vfs_fstat(file, &stat);
  if (status != VFS_STATUS_OK) {
    free(path_copy);
    vfs_close(file);
    return -vfs_status_to_errno(status);
  }

  void* buffer = calloc(1, stat->size);
  if (buffer == NULL) {
    free(path_copy);
    free(stat);
    vfs_close(file);
    return -ENOMEM;
  }

  uint64_t bytes_read = 0;
  status = vfs_read(file, buffer, stat->size, &bytes_read);
  vfs_close(file);
  if (status != VFS_STATUS_OK || bytes_read != stat->size) {
    free(path_copy);
    free(stat);
    free(buffer);
    return status == VFS_STATUS_OK ? -EINVAL : -vfs_status_to_errno(status);
  }

  elf_t* elf = elf_read(buffer, stat->size);
  free(stat);
  if (elf == NULL) {
    free(path_copy);
    free(buffer);
    return -EINVAL;
  }

  long ret = sched_execve(
      g_kernel.current_process, elf, path_copy, strlen(path_copy));
  free(path_copy);
  if (ret != 0) {
    elf_free(elf);
    free(buffer);
  }
  return ret;
}
