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

#define EXECVE_ARG_MAX 256

static void free_string_array(char** strings) {
  if (strings == NULL) { return; }

  for (uint64_t i = 0; strings[i] != NULL; ++i) { free(strings[i]); }
  free(strings);
}

static long copy_string_array(char* const user_strings[],
                              char*** strings_out,
                              uint64_t* count_out) {
  if (strings_out == NULL || count_out == NULL) { return -EINVAL; }

  *strings_out = NULL;
  *count_out = 0;

  if (user_strings == NULL) {
    char** strings = calloc(1, sizeof(char*));
    if (strings == NULL) { return -ENOMEM; }
    *strings_out = strings;
    return 0;
  }

  uint64_t count = 0;
  for (; count < EXECVE_ARG_MAX; ++count) {
    char* const* user_slot = &user_strings[count];
    if (!syscall_is_uaddr(user_slot, sizeof(char*))) { return -EINVAL; }
    if (*user_slot == NULL) { break; }
  }
  if (count == EXECVE_ARG_MAX) { return -EINVAL; }

  char** strings = calloc(count + 1, sizeof(char*));
  if (strings == NULL) { return -ENOMEM; }

  for (uint64_t i = 0; i < count; ++i) {
    char* const* user_slot = &user_strings[i];
    if (!syscall_is_uaddr(*user_slot, SYSCALL_USER_STRING_MAX)) {
      free_string_array(strings);
      return -EINVAL;
    }

    strings[i] = syscall_utok_strcpy(*user_slot, SYSCALL_USER_STRING_MAX);
    if (strings[i] == NULL) {
      free_string_array(strings);
      return -ENOMEM;
    }
  }

  *strings_out = strings;
  *count_out = count;
  return 0;
}

long syscall_execve(const char* pathname,
                    char* const argv[],
                    char* const envp[]) {
  if (pathname == NULL) { return -EINVAL; }
  if (!syscall_is_uaddr(pathname, SYSCALL_USER_STRING_MAX)) { return -EINVAL; }

  char* path_copy = syscall_utok_strcpy(pathname, SYSCALL_USER_STRING_MAX);
  if (path_copy == NULL) { return -ENOMEM; }

  char** argv_copy = NULL;
  char** envp_copy = NULL;
  uint64_t argc = 0;
  uint64_t envc = 0;
  long copy_status = copy_string_array(argv, &argv_copy, &argc);
  if (copy_status != 0) {
    free(path_copy);
    return copy_status;
  }

  copy_status = copy_string_array(envp, &envp_copy, &envc);
  if (copy_status != 0) {
    free_string_array(argv_copy);
    free(path_copy);
    return copy_status;
  }
  (void)envc;

  vfs_file_t* file = NULL;
  vfs_status_t status = vfs_get_file_handle(path_copy, VFS_OPEN_READ, &file);
  if (status != VFS_STATUS_OK) {
    free_string_array(envp_copy);
    free_string_array(argv_copy);
    free(path_copy);
    return -vfs_status_to_errno(status);
  }

  vfs_stat_t* stat = NULL;
  status = vfs_fstat(file, &stat);
  if (status != VFS_STATUS_OK) {
    free_string_array(envp_copy);
    free_string_array(argv_copy);
    free(path_copy);
    vfs_close(file);
    return -vfs_status_to_errno(status);
  }

  void* buffer = calloc(1, stat->size);
  if (buffer == NULL) {
    free_string_array(envp_copy);
    free_string_array(argv_copy);
    free(path_copy);
    free(stat);
    vfs_close(file);
    return -ENOMEM;
  }

  uint64_t bytes_read = 0;
  status = vfs_read(file, buffer, stat->size, &bytes_read);
  vfs_close(file);
  if (status != VFS_STATUS_OK || bytes_read != stat->size) {
    free_string_array(envp_copy);
    free_string_array(argv_copy);
    free(path_copy);
    free(stat);
    free(buffer);
    return status == VFS_STATUS_OK ? -EINVAL : -vfs_status_to_errno(status);
  }

  elf_t* elf = elf_read(buffer, stat->size);
  free(stat);
  if (elf == NULL) {
    free_string_array(envp_copy);
    free_string_array(argv_copy);
    free(path_copy);
    free(buffer);
    return -EINVAL;
  }

  long ret = sched_execve(g_kernel.current_process,
                          elf,
                          path_copy,
                          strlen(path_copy),
                          argc,
                          argv_copy,
                          envp_copy);
  free(path_copy);
  if (ret != 0) {
    elf_free(elf);
    free(buffer);
  }
  return ret;
}
