#ifndef HOJICHA_CPU_FPU_H
#define HOJICHA_CPU_FPU_H

#include <stdbool.h>

typedef struct fpu fpu_t;

bool fpu_initialize(void);
fpu_t* fpu_new(void);
void fpu_free(fpu_t* fpu);
void fpu_reset(fpu_t* fpu);
void fpu_copy(fpu_t* dst, const fpu_t* src);
void fpu_save(fpu_t* fpu);
void fpu_restore(fpu_t* fpu);

#endif  // HOJICHA_CPU_FPU_H
