#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/hojicha.kernel isodir/boot/hojicha.kernel
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "hojicha" {
	multiboot /boot/hojicha.kernel
}
EOF
grub-mkrescue -o hojicha.iso isodir
