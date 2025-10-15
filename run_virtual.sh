#!/bin/sh
set -e
. ./make_iso.sh "$@"

echo "Starting QEMU with QEMU_ARGS: $QEMU_ARGS"
if [[ "$*" == *"--debug-qemu"* ]]
then
    qemu-system-$(./target_triplet_to_arch.sh $HOST) -cdrom hojicha.iso -m 1G $QEMU_ARGS -s -S
else
    qemu-system-$(./target_triplet_to_arch.sh $HOST) -cdrom hojicha.iso -m 1G $QEMU_ARGS
fi


