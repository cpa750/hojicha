#ifndef HOJICHA_INITRD_H
#define HOJICHA_INITRD_H

#include <fs/vfs.h>

/*
 * Create an initrd VFS mount from a USTAR image.
 */
vfs_status_t initrd_from_ustar(void* buffer,
                               uint64_t size,
                               vfs_mount_t** mount_out);

#endif  // HOJICHA_INITRD_H

