#include <dev/chardev.h>
#include <dev/null.h>
#include <dev/zero.h>
#include <fs/devfs.h>
#include <stddef.h>

void chardev_initialize(void) {
  devfs_device_t* null_dev = null_dev_new();
  if (null_dev != NULL) {
    devfs_register(DEVFS_CHARDEV, 0, null_dev, "null", 4);
  }

  devfs_device_t* zero_dev = zero_dev_new();
  if (zero_dev != NULL) {
    devfs_register(DEVFS_CHARDEV, 1, zero_dev, "zero", 4);
  }
}
