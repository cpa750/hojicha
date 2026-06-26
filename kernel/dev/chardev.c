#include <dev/chardev.h>
#include <dev/console.h>
#include <dev/fb.h>
#include <dev/null.h>
#include <dev/zero.h>
#include <drivers/tty.h>
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

  console_initialize();
  tty_device_initialize();

  devfs_device_t* fb_dev = fb_dev_new();
  if (fb_dev != NULL) {
    devfs_register(DEVFS_CHARDEV, 4, fb_dev, "fb0", 3);
  }
}
