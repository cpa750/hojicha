target_sources(kernel_objects PRIVATE
  elf.c
  kernel.c
  ktime.c
  g_kernel.c
)

add_subdirectory(syscalls)
