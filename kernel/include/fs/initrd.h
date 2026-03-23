#ifndef HOJICHA_INITRD_H
#define HOJICHA_INITRD_H

#include <fs/vfs.h>

/*
 * Create an initrd VFS mount from a USTAR image.
 */
vfs_status_t initrd_from_ustar(void* buffer,
                               uint64_t size,
                               vfs_mount_t** mount_out);

vfs_status_t initrd_lookup(vfs_node_t* dir,
                           const char* name,
                           uint32_t name_len,
                           vfs_node_t** out);
vfs_status_t initrd_open(vfs_node_t* vnode, uint32_t flags, vfs_file_t** out);
vfs_status_t initrd_read(vfs_file_t* vfile,
                         void* buffer,
                         uint64_t len,
                         uint64_t* bytes_read_out);
void initrd_release(vfs_node_t* vnode);
vfs_status_t initrd_stat(vfs_node_t* vnode, vfs_stat_t** out);

#endif  // HOJICHA_INITRD_H

