#!/bin/sh
set -e
. ./make_iso.sh "$@"

mkdir -p logs
QEMU_ARGS="$QEMU_ARGS -serial file:logs/serial.log"

echo "Starting QEMU with QEMU_ARGS: $QEMU_ARGS"
case "$*" in
  *--debug-qemu*)
    qemu-system-$(./target_triplet_to_arch.sh $HOST) -cdrom hojicha.iso -m 1G -vga std -global VGA.edid=on -global VGA.xres=1920 -global VGA.yres=1080 $QEMU_ARGS -s -S -no-shutdown -no-reboot -d int
    ;;
  *)
    qemu-system-$(./target_triplet_to_arch.sh $HOST) -cdrom hojicha.iso -m 1G -vga std -global VGA.edid=on -global VGA.xres=1920 -global VGA.yres=1080 $QEMU_ARGS
    ;;
esac
