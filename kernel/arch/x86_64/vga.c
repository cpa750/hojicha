#include <drivers/vga.h>
#include <limine.h>
#include <stdint.h>
#include <stdlib.h>

__attribute__((
    used,
    section(
        ".limine_requests"))) static volatile struct limine_framebuffer_request
    framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

struct vga_state {
  uint32_t pitch;
  uint32_t* framebuffer_addr;
};
uint32_t* vga_state_get_framebuffer_addr(vga_state_t* v) {
  return v->framebuffer_addr;
}

void vga_initialize(void) {
  if (framebuffer_request.response == NULL ||
      framebuffer_request.response->framebuffer_count < 1) {
    abort();
  }
  struct limine_framebuffer* framebuffer =
      framebuffer_request.response->framebuffers[0];
  for (size_t i = 0; i < 250; i++) {
    volatile uint32_t* fb_ptr = framebuffer->address;
    fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
  }
}

void vga_set_pixel(uint32_t x, uint32_t y, vga_color_t color) {}

