target_sources(kernel_objects PRIVATE
  entry.asm
  enter_ring3.asm
  load_gdt.asm
  load_idt.asm
  load_pd.asm
  load_tss.asm
  isr.asm
  irq.asm
  make_fork_kstack.asm
  switch_to.asm
  vga.c
)
