#ifndef HOJICHA_MULTITASK_SYSCALL_UTILS_H
#define HOJICHA_MULTITASK_SYSCALL_UTILS_H

#include <fs/vfs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SYSCALL_USER_STRING_MAX   4096
#define SYSCALL_HIGHER_HALF_START 0xFFFF000000000000ULL

long syscall_lookup_parent_for_child(const char* path,
                                     vfs_node_t** parent_out,
                                     const char** name_out,
                                     uint32_t* name_len_out);
vfs_status_t syscall_close_fd(long fd);
bool syscall_is_uaddr(const void* ptr, size_t len);
void* syscall_utok_memcpy(const void* src, size_t len);
char* syscall_utok_strcpy(const char* src, size_t max_len);

#endif  // HOJICHA_MULTITASK_SYSCALL_UTILS_H
