#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#define FB_PATH   "/dev/fb0"
#define FB_WIDTH  1920
#define FB_HEIGHT 1080
#define FB_PITCH  (FB_WIDTH * 4)
#define FB_SIZE   (FB_PITCH * FB_HEIGHT)

static uint32_t gamut_color(uint32_t x, uint32_t y) {
  uint32_t r = (x * 255) / (FB_WIDTH - 1);
  uint32_t g = (y * 255) / (FB_HEIGHT - 1);
  uint32_t b = ((x + y) * 255) / (FB_WIDTH + FB_HEIGHT - 2);

  return (r << 16) | (g << 8) | b;
}

int main(void) {
  int fd = open(FB_PATH, O_RDWR, 0);
  if (fd < 0) {
    printf("fbrainbow: cannot open %s: %d\n", FB_PATH, errno);
    return 1;
  }

  uint32_t* fb = mmap(
      NULL, FB_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fb == MAP_FAILED) {
    printf("fbrainbow: cannot mmap %s: %d\n", FB_PATH, errno);
    close(fd);
    return 1;
  }

  for (uint32_t y = 0; y < FB_HEIGHT; ++y) {
    uint32_t* row = (uint32_t*)((uint8_t*)fb + (y * FB_PITCH));
    for (uint32_t x = 0; x < FB_WIDTH; ++x) {
      row[x] = gamut_color(x, y);
    }
  }

  uint32_t first_pixel = 0;
  if (lseek(fd, 0, SEEK_SET) < 0 ||
      read(fd, &first_pixel, sizeof(first_pixel)) != sizeof(first_pixel)) {
    printf("fbrainbow: cannot verify framebuffer write: %d\n", errno);
  } else if (first_pixel != gamut_color(0, 0)) {
    printf("fbrainbow: mmap verification failed: got %x expected %x\n",
           first_pixel,
           gamut_color(0, 0));
  }

  close(fd);
  return 0;
}
