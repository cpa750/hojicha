#!/bin/sh
set -e
. ./make_iso.sh

echo "QEMU_ARGS: $QEMU_ARGS"
qemu-system-$(./target_triplet_to_arch.sh $HOST) -cdrom hojicha.iso $QEMU_ARGS

