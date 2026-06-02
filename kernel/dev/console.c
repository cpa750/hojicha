#include <dev/console.h>
#include <drivers/tty.h>
#include <fs/devfs.h>
#include <fs/vfs.h>
#include <fs/vfs_utils.h>
#include <kernel/g_kernel.h>
#include <stdbool.h>
#include <stdlib.h>

vfs_status_t console_write(vfs_file_t* file,
                           void* buffer,
                           uint64_t len,
                           uint64_t* bytes_written_out);
static vfs_status_t console_close(vfs_file_t* file);

typedef struct console_state console_state_t;
struct console_state {
  devfs_device_t* dev;
  bool is_initialized;
};

void console_initialize(void) {
  vfs_file_ops_t* file_ops = calloc(1, sizeof(vfs_file_ops_t));
  vfs_node_ops_t* node_ops = calloc(1, sizeof(vfs_node_ops_t));
  if (file_ops == NULL || node_ops == NULL) {
    free(file_ops);
    free(node_ops);
    return;
  }

  file_ops->write = console_write;
  file_ops->close = console_close;

  devfs_device_t* dev = devfs_device_new(file_ops, node_ops);
  if (dev == NULL) {
    free(file_ops);
    free(node_ops);
    return;
  }

  console_state_t* state = calloc(1, sizeof(console_state_t));
  if (state == NULL) {
    free(dev);
    free(file_ops);
    free(node_ops);
    return;
  }

  if (devfs_register(DEVFS_CHARDEV, 2, dev, "console", 7) != VFS_STATUS_OK) {
    free(state);
    free(dev);
    free(file_ops);
    free(node_ops);
    return;
  }

  state->dev = dev;
  state->is_initialized = true;
  g_kernel.console = state;
  return;
}

static vfs_status_t console_close(vfs_file_t* file) {
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }

  file->fs_data = NULL;
  return VFS_STATUS_OK;
}

vfs_status_t console_write(vfs_file_t* file,
                           void* buffer,
                           uint64_t len,
                           uint64_t* bytes_written_out) {
  (void)file;
  // More kludges? Yes please!!
  terminal_write(buffer, len);
  SET_OUT(bytes_written_out, len);
  return VFS_STATUS_OK;
}
