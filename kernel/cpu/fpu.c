#include <cpu/fpu.h>
#include <memory/slab.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define FPU_FXSAVE_SIZE  512
#define FPU_FXSAVE_ALIGN 16

struct fpu {
  void* raw_area;
  void* fxsave_area;
};

extern bool fpu_cpu_initialize(void);
extern void fpu_save_area(void* fxsave_area);
extern void fpu_restore_area(void* fxsave_area);

static uint8_t initial_fxsave_area[FPU_FXSAVE_SIZE]
    __attribute__((aligned(FPU_FXSAVE_ALIGN)));

static uintptr_t fpu_align_up(uintptr_t addr, uintptr_t alignment) {
  return (addr + alignment - 1) & ~(alignment - 1);
}

bool fpu_initialize(void) {
  if (!fpu_cpu_initialize()) { return false; }
  fpu_save_area(initial_fxsave_area);
  return true;
}

fpu_t* fpu_new(void) {
  fpu_t* fpu = slab_calloc(sizeof(fpu_t));
  void* raw_area = slab_calloc(FPU_FXSAVE_SIZE + FPU_FXSAVE_ALIGN - 1);
  if (fpu == NULL || raw_area == NULL) {
    slab_free(fpu);
    slab_free(raw_area);
    return NULL;
  }

  fpu->raw_area = raw_area;
  fpu->fxsave_area =
      (void*)fpu_align_up((uintptr_t)raw_area, FPU_FXSAVE_ALIGN);
  fpu_reset(fpu);
  return fpu;
}

void fpu_free(fpu_t* fpu) {
  if (fpu == NULL) { return; }
  slab_free(fpu->raw_area);
  slab_free(fpu);
}

void fpu_reset(fpu_t* fpu) {
  if (fpu == NULL || fpu->fxsave_area == NULL) { return; }
  memcpy(fpu->fxsave_area, initial_fxsave_area, FPU_FXSAVE_SIZE);
}

void fpu_copy(fpu_t* dst, const fpu_t* src) {
  if (dst == NULL || src == NULL || dst->fxsave_area == NULL ||
      src->fxsave_area == NULL) {
    return;
  }
  memcpy(dst->fxsave_area, src->fxsave_area, FPU_FXSAVE_SIZE);
}

void fpu_save(fpu_t* fpu) {
  if (fpu == NULL || fpu->fxsave_area == NULL) { return; }
  fpu_save_area(fpu->fxsave_area);
}

void fpu_restore(fpu_t* fpu) {
  if (fpu == NULL || fpu->fxsave_area == NULL) { return; }
  fpu_restore_area(fpu->fxsave_area);
}
