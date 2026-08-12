[BITS 64]

%define CPUID_FEAT_EDX_FPU       (1 << 0)
%define CPUID_FEAT_EDX_FXSR      (1 << 24)
%define CPUID_FEAT_EDX_SSE       (1 << 25)
%define CR0_MP                   (1 << 1)
%define CR0_EM                   (1 << 2)
%define CR0_TS                   (1 << 3)
%define CR0_NE                   (1 << 5)
%define CR0_CLEAR_EM_TS_MASK     0xfffffffffffffff3
%define CR4_OSFXSR               (1 << 9)
%define CR4_OSXMMEXCPT           (1 << 10)

global fpu_cpu_initialize
global fpu_save_area
global fpu_restore_area

section .text

;; Used with modifications from https://wiki.osdev.org/FPU
fpu_cpu_initialize:
  push rbx

  mov eax, 1
  cpuid
  test edx, CPUID_FEAT_EDX_FPU
  jz .nofpu
  test edx, CPUID_FEAT_EDX_FXSR
  jz .nofpu
  test edx, CPUID_FEAT_EDX_SSE
  jz .nofpu

  mov rax, cr0
  and rax, CR0_CLEAR_EM_TS_MASK
  or rax, (CR0_MP | CR0_NE)
  mov cr0, rax

  mov rax, cr4
  or rax, (CR4_OSFXSR | CR4_OSXMMEXCPT)
  mov cr4, rax

  fninit
  ldmxcsr [rel default_mxcsr]
  mov eax, 1
  pop rbx
  ret

.nofpu:
  xor eax, eax
  pop rbx
  ret

fpu_save_area:
  test rdi, rdi
  jz .save_done
  fxsave64 [rdi]
.save_done:
  ret

fpu_restore_area:
  test rdi, rdi
  jz .restore_done
  fxrstor64 [rdi]
.restore_done:
  ret

section .rodata align=16
  default_mxcsr: dd 0x1f80
