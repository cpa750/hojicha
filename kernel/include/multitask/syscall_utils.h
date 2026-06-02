#ifndef HOJICHA_MULTITASK_SYSCALL_UTILS_H
#define HOJICHA_MULTITASK_SYSCALL_UTILS_H

#include <fs/vfs.h>
#include <stdint.h>

long syscall_lookup_parent_for_child(const char* path,
                                     vfs_node_t** parent_out,
                                     const char** name_out,
                                     uint32_t* name_len_out);

#endif  // HOJICHA_MULTITASK_SYSCALL_UTILS_H
