#include <drivers/vga.h>
#include <fonts/inconsolata.h>
#include <haddr.h>
#include <kernel/kernel_state.h>
#include <limine.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((
    used,
    section(
        ".limine_requests"))) static volatile struct limine_framebuffer_request
    framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

struct vga_state {
  uint64_t height;
  uint64_t width;
  uint8_t bpp;
  uint64_t pitch;
  uint32_t* framebuffer_addr;
  uint32_t* framebuffer_end;
};
typedef struct vga_state vga_state_t;
uint64_t vga_state_get_height(vga_state_t* v) { return v->height; }
uint64_t vga_state_get_width(vga_state_t* v) { return v->width; }
uint8_t vga_state_get_bpp(vga_state_t* v) { return v->bpp; }
uint64_t vga_state_get_pitch(vga_state_t* v) { return v->pitch; }
uint32_t* vga_state_get_framebuffer_addr(vga_state_t* v) {
  return v->framebuffer_addr;
}
uint32_t* vga_state_get_framebuffer_end(vga_state_t* v) {
  return v->framebuffer_end;
}
void vga_state_dump(vga_state_t* v) {
  printf("[VGA] Height:\t\t\t\t%d B\n", (uint64_t)v->height);
  printf("[VGA] Width:\t\t\t\t%d B\n", (uint64_t)v->width);
  printf("[VGA] BPP:\t\t\t\t%d B\n", (uint8_t)v->bpp);
  printf("[VGA] Pitch:\t\t\t\t%d B\n", (uint64_t)v->pitch);
  printf("[VGA] Framebuffer address:\t\t\t\t%d B\n",
         (haddr_t)v->framebuffer_addr);
}

void vga_initialize(void) {
  static vga_state_t vga = {0};
  if (framebuffer_request.response == NULL ||
      framebuffer_request.response->framebuffer_count < 1) {
    abort();
  }
  struct limine_framebuffer* framebuffer =
      framebuffer_request.response->framebuffers[0];
  vga.framebuffer_addr = framebuffer->address;
  vga.bpp = framebuffer->bpp;
  vga.height = framebuffer->height;
  vga.pitch = framebuffer->pitch;
  vga.width = framebuffer->width;
  vga.framebuffer_end =
      framebuffer->address + (framebuffer->pitch * framebuffer->height);
  g_kernel.vga = &vga;
}

void vga_set_pixel(vga_position_t* pos, rgb32_t color) {
  g_kernel.vga
      ->framebuffer_addr[pos->y * (g_kernel.vga->pitch >> 2) + pos->x - 1] =
      color;
}

/*
 * Draws a bitmap to the screen. The bitmap must have 16 elements.
 */
void vga_draw_bitmap_16h8w(vga_position_t* start_pos, uint8_t* bitmap16,
                           rgb32_t color) {
  // TODO Performance improvements
  if (start_pos->x >= g_kernel.vga->width - 7 || start_pos->x < 0 ||
      start_pos->y >= g_kernel.vga->height - 15 || start_pos->y < 0) {
    return;
  }

  for (uint8_t bitmap_row = 0; bitmap_row < 16; ++bitmap_row) {
    for (uint8_t bitmap_column = 0; bitmap_column < 8; ++bitmap_column) {
      vga_position_t pos = {start_pos->x + (8 - bitmap_column),
                            start_pos->y + bitmap_row};
      if (bitmap16[bitmap_row] & 1 << bitmap_column) {
        vga_set_pixel(&pos, color);
      } else {
        vga_set_pixel(&pos, 0);
      }
    }
  }
}

