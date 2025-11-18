#ifndef VGA_H
#define VGA_H

#include <stdint.h>

enum vga_color {
  VGA_COLOR_BLACK = 0,
  VGA_COLOR_BLUE = 1,
  VGA_COLOR_GREEN = 2,
  VGA_COLOR_CYAN = 3,
  VGA_COLOR_RED = 4,
  VGA_COLOR_MAGENTA = 5,
  VGA_COLOR_BROWN = 6,
  VGA_COLOR_LIGHT_GREY = 7,
  VGA_COLOR_DARK_GREY = 8,
  VGA_COLOR_LIGHT_BLUE = 9,
  VGA_COLOR_LIGHT_GREEN = 10,
  VGA_COLOR_LIGHT_CYAN = 11,
  VGA_COLOR_LIGHT_RED = 12,
  VGA_COLOR_LIGHT_MAGENTA = 13,
  VGA_COLOR_LIGHT_BROWN = 14,
  VGA_COLOR_WHITE = 15,
};
typedef enum vga_color vga_color_t;
typedef uint32_t rgb32_t;

struct vga_state;
typedef struct vga_state vga_state_t;
uint64_t vga_state_get_height(vga_state_t* v);
uint64_t vga_state_get_width(vga_state_t* v);
uint8_t vga_state_get_bpp(vga_state_t* v);
uint64_t vga_state_get_pitch(vga_state_t* v);
uint32_t* vga_state_get_framebuffer_addr(vga_state_t* v);
uint32_t* vga_state_get_framebuffer_end(vga_state_t* v);
void vga_state_dump(vga_state_t* v);

struct vga_position {
  uint32_t x;
  uint32_t y;
};
typedef struct vga_position vga_position_t;

typedef uint32_t vga_data_t;

void vga_initialize(void);
void vga_draw_bitmap_16h8w(vga_position_t* start_pos, uint8_t* bitmap16,
                           rgb32_t color);
void vga_draw_rect_solid(vga_position_t* top_left, vga_position_t* bottom_right,
                         rgb32_t color);
void vga_set_pixel(vga_position_t* pos, rgb32_t color);
void vga_copy_region_to_buffer(vga_position_t* top_left,
                               vga_position_t* bottom_right,
                               vga_data_t* buffer);
void vga_copy_buffer_to_region(vga_position_t* top_left,
                               vga_position_t* bottom_right,
                               vga_data_t* buffer);

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
  return fg | bg << 4;
}
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
  return (uint16_t)uc | (uint16_t)color << 8;
}

#endif

