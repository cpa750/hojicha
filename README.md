# Hojicha
This project is a first effort at developing an operating system for the x86_64 platform. Thanks for checking it out!

> Caution: Hojicha is _very much_ a work in progress. It is very incomplete, and likely has many bugs. _Please do not_
> try to install it on real hardware - I'm not responsible for what happens if you do.

## Current Status
Under (early) active development.

## Current goal:
A first go at a scheduler
### In progress:
- Implement timeslices
### Planned:
- Process termination and cleanup
- Semaphores
- IPC
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
- Framebuffer TTY
- Migrate to x86_64

If you're reading this and you have feedback, please don't hesitate to let me know!

#### TODO:
Build and run instructions (basically get an x86 cross compiler and run `./run_virtual.sh`)
