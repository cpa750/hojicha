# Hojicha
This project is a first effort at developing an operating system for the x86_64 platform. Thanks for checking it out!

> Caution: Hojicha is _very much_ a work in progress. It is very incomplete, and likely has many bugs. _Please do not_
> try to install it on real hardware - I'm not responsible for what happens if you do.

## Current Status:
Preparation work for proper init and shell programs:
- [X] VFS exposed as syscalls
- [x] TTY configuration via devfs
- [x] stdin/stdout/stderr
- [ ] fork()/exec()
- [ ] Userland malloc()/free(), mmap(), sbrk()

## MVP Done:
- [x] Babysteps
  - [x] Boot hello world
  - [x] Basic TTY output
  - [x] Serial debug logs
  - [x] GDT
- [x] Interrupts
  - [x] IDT, simple PIC driver
  - [x] Timer interrupts
  - [x] Keyboard input
- [x] Memory management
  - [x] Physical memory manager
  - [x] Virtual memory manager (paging)
  - [x] Basic kmalloc()/kfree()
- [x] Framebuffer TTY
- [x] x86_64 migration
- [x] Round robin scheduler
- [x] Executable (ELF) loading
  - [x] Statically linked (LD_LOAD) executables
  - [ ] Dynamically linked executables/libraries
- [x] Syscall machinery
  - [x] exit
  - [x] sleep/nanosleep
  - [x] VFS functionality
  - [ ] Many, many others
- [x] Filesystems
  - [x] VFS prototype
    - [ ] Relative paths
  - [x] initrd from bundled USTAR image
  - [x] devfs machinery prototype
  - [ ] Ext2
- [ ] Userspace
  - [ ] ls/touch/mkdir/rm/cat etc.
  - [ ] Brainfuck interpreter/compiler
  - [ ] DOOM port

If you're reading this and you have feedback, please don't hesitate to let me know!

## Building & Running:
### Prerequisites
- GCC cross-compiler targeting x86_64 ELF - there are good instructions for this at https://wiki.osdev.org/GCC_Cross-Compiler. I personally am building with v15.2.0 - other versions _should_ work as well, but no guarantees.
- CMake
- Ninja
- NASM
- Xorriso
- QEMU - Other emulators likely work as well, but the build/run scripts are built around using QEMU for emulation.

It then should be as simple as `./run_virtual.sh` from the project root - see below for notes on build flags.

### Build Flags
- You can set the default kernel log level by passing `--hlog-level=INFO` (or `WARN`, `ERROR`, `FATAL`, `DEBUG`, `VERBOSE`) to `./build.sh` or `./run_virtual.sh`.
- You can enable debugging via QEMU by passing `--debug-qemu` to `./build.sh` and `./run_virtual.sh`.
- Tests can be run with: `--test-chardev`, `--test-kmalloc`, `--test-initrd`, `--test-vfs`, `--test-ringbuffer`, or `--test-all`
- Kmalloc stress tests can be run with: `--stress-kmalloc`
- Automated system tests can be enabled with `--ast-scheduler`. The results can be verified with `scripts/verify_scheduler_ast.py`.
- `./run_virtual.sh` writes QEMU serial output to `logs/serial.log` by default. Override `QEMU_ARGS` to choose a different location.
