#!/bin/sh
set -e
. ./config.sh

rm -f  -- compile_commands.json
rm -rf build
rm -rf userspace/bin
rm -rf initrd/bin
rm -rf sysroot
rm -rf iso_root
rm -rf hojicha.iso
