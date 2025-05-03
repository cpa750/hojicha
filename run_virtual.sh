#!/bin/sh
set -e
. ./make_iso.sh

qemu-system-$(./target_triplet_to_arch.sh $HOST) -cdrom hojicha.iso

