# Hojicha
This project is a first effort at developing an operating system for the x86_64 platform. Thanks for checking it out!

> Caution: Hojicha is _very much_ a work in progress. It is very incomplete, and likely has many bugs. _Please do not_
> try to install it on real hardware - I'm not responsible for what happens if you do.

## Current Status
Under (early) active development.

## Current Goal:
Executable loading and first steps into userspace

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
- Round robin scheduling

If you're reading this and you have feedback, please don't hesitate to let me know!

## Building & Running:
### Prerequisites
- GCC cross-compiler targeting x86_64 ELF - there are good instructions for this at https://wiki.osdev.org/GCC_Cross-Compiler. I personally am building with v15.2.0 - other versions _should_ work as well, but no guarantees.
- NASM
- Xorriso
- QEMU - Other emulators likely work as well, but the build/run scripts are built around using QEMU for emulation.

It then should be as simple as `./run_virtual.sh` from the project root - see below for notes on build flags.

### Build Flags
You can set the default kernel log level by passing `--hlog-level=INFO` (or `WARN`, `ERROR`, `FATAL`, `DEBUG`, `VERBOSE`) to `./build.sh` or `./run_virtual.sh`.
You can enable debugging via QEMU by passing `--debug-qemu` to `./build.sh` and `./run_virtual.sh`.
