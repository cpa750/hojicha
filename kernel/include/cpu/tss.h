#ifndef TSS_H
#define TSS_H

#include <stdint.h>

typedef struct {
  uint16_t link_low;
  uint16_t link_high;
  uint32_t esp0;
  uint16_t ss0_low;
  uint16_t ss0_high;
  uint32_t esp1;
  uint16_t ss1_low;
  uint16_t ss1_high;
  uint32_t esp2;
  uint16_t ss2_low;
  uint16_t ss2_high;
  uint32_t cr3;
  uint32_t eip;
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint16_t es_low;
  uint16_t es_high;
  uint16_t cs_low;
  uint16_t cs_high;
  uint16_t ss_low;
  uint16_t ss_high;
  uint16_t ds_low;
  uint16_t ds_high;
  uint16_t fs_low;
  uint16_t fs_high;
  uint16_t gs_low;
  uint16_t gs_high;
  uint16_t ldt_segment_selector_low;
  uint16_t ldt_segment_selector_high;
  uint16_t io_map_base_low;
  uint16_t io_map_base_high;
} __attribute__((packed)) TSS;

#endif  // TSS_H

