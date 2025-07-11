#include <cpu/protected.h>

bool is_protected_mode() {
  uint32_t cr0;
  asm volatile("mov %%cr0, %0" : "=r"(cr0));
  return (bool)(cr0 & 0b1);
}
