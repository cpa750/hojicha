# Hojicha
This project is a first effort at developing an operating system for the i386 platform. Thanks for checking it out!

> Caution: Hojicha is _very much_ a work in progress. It is very incomplete, and likely has many bugs. _Please do not_
> try to install it on real hardware - I'm not responsible for what happens if you do.

## Current Status
Under (early) active development.

## Current goal:
Migrate to x86_64.
- Implement TTY with a framebuffer
### Planned:
- Refactor GDT/IDT code to work with x86_64
- Higher half kernel (if required by chosen bootloader)
- Refactor PMM code for 64 bits
- Refactor VMM for 64 bits, implement 4 level paging directory
- Migrate build to CMake
### In progress:

## MVP Done:
- Boot hello world
- Basic TTY output
- Serial debug logs
- GDT
- IDT, simple PIC driver
- Timer interrupts
- Keyboard input
- Physical memory manager
- Virtual memory manager (paging)
- Basic malloc()/free()

If you're reading this and you have feedback, please don't hesitate to let me know!

#### TODO:
Build and run instructions (basically get an x86 cross compiler and run `./run_virtual.sh`)
