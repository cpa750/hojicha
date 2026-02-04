#!/bin/sh

HOST=x86_64-elf

set -e
. ./headers.sh

DEBUG_FLAG=" "
if [[ "$*" == *"--debug-qemu"* ]]; then
    DEBUG_QEMU="DEBUG_QEMU=1"
fi

KMALLOC_TEST=" "
if [[ "$*" == *"--kmalloc-test"* ]]; then
    KMALLOC_TEST="KMALLOC_TEST=1"
fi

for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" bear --append -- $MAKE $DEBUG_QEMU $KMALLOC_TEST HOST=$HOST install)
done

jq -s 'add' $(printf '%s/compile_commands.json ' $PROJECTS) > compile_commands.json
